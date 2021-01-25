
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <U8g2lib.h>
#include <string.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#include "main.h"  // network information

// function prototypes
void writeToDisplay(U8G2_MAX7219_64X8_F_4W_SW_SPI* u8g2R, char* buffer);
void requestData(WiFiClient* client);
void processResponse(int* state, String* line, int* i);
void printResultsLED(void);

U8G2_MAX7219_64X8_F_4W_SW_SPI u8g2R0(U8G2_R0, /* clock=*/16, /* data=*/5, /* cs=*/4, /* dc=*/U8X8_PIN_NONE, /* reset=*/U8X8_PIN_NONE);
U8G2_MAX7219_64X8_F_4W_SW_SPI u8g2R1(U8G2_R0, /* clock=*/16, /* data=*/5, /* cs=*/0, /* dc=*/U8X8_PIN_NONE, /* reset=*/U8X8_PIN_NONE);
U8G2_MAX7219_64X8_F_4W_SW_SPI u8g2R2(U8G2_R0, /* clock=*/16, /* data=*/5, /* cs=*/14, /* dc=*/U8X8_PIN_NONE, /* reset=*/U8X8_PIN_NONE);
U8G2_MAX7219_64X8_F_4W_SW_SPI u8g2R3(U8G2_R0, /* clock=*/16, /* data=*/5, /* cs=*/12, /* dc=*/U8X8_PIN_NONE, /* reset=*/U8X8_PIN_NONE);
// U8G2_MAX7219_16X16_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ 11, /* data=*/ 12, /* cs=*/ 10, /* dc=*/ U8X8_PIN_NONE, /* reset=*/ U8X8_PIN_NONE);

U8G2_MAX7219_64X8_F_4W_SW_SPI u8g2Lines[4] = {u8g2R0, u8g2R1, u8g2R2, u8g2R3};

WiFiClient client;
char receive[256];  // 0..255
char flag = 0;
unsigned long act_time;  // wifi grabber time
unsigned int display_page;
unsigned long page_timer;

#define NO_TRIPS 18  // number of trips to save
int countdown[NO_TRIPS];
int bimline[NO_TRIPS];
String dest[NO_TRIPS];

void setup(void) {
    // initialize timing variable
    act_time = millis();
    page_timer = 0;
    display_page = 0;

    Serial.begin(115200);

    // for network information include main.h
    //const char ssid[] = "XXXX";
    //const char pass[] = "XXXX";

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    // connection with timeout
    int count = 0;
    while ((WiFi.status() != WL_CONNECTED) && count < 17) {
        Serial.print(".");
        delay(500);
        count++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("");
        Serial.print(F("Failed to connect to "));
        Serial.println(ssid);
        while (1)
            ;
    }

    Serial.println("");
    Serial.println(F("[CONNECTED]"));
    Serial.print("[IP ");
    Serial.print(WiFi.localIP());
    Serial.println("]");

    // assign contrast value to all lines
    for (int i = 0; i < 4; i++) {
        (u8g2Lines[i]).begin();
        (u8g2Lines[i]).setContrast(4);
    }
}

void loop(void) {
    if (millis() > act_time + (unsigned long)20000) {
        act_time = millis();

        WiFiClient client;
        // allocate trip variables
        requestData(&client);

        int i = 0;
        int state = 0;
        while (client.connected() || client.available()) {
            if (client.available()) {
                // read string from website
                // and grab departure information

                String line = client.readStringUntil('>');

                // Serial.println(line);
                processResponse(&state, &line, &i);
                if (i > NO_TRIPS - 1) {  // prevent overflow of arrays
                    client.stop();
                    break;
                }

                yield();  // let ESP do some background stuff e.g. networking
            }
        }
        // stopping client connection
        client.stop();
        Serial.println("\n[Disconnected]");
    }
    if (millis() > page_timer + 3000) {
        page_timer = millis();

        printResultsLED();
    }
}

void writeToDisplay(U8G2_MAX7219_64X8_F_4W_SW_SPI* u8g2R, char* buffer) {
    // writes buffer to one line of a matrix display
    u8g2R->clearBuffer();              // clear the internal memory
    u8g2R->setFont(u8g2_font_5x7_mf);  // choose a suitable font
    u8g2R->drawStr(0, 7, buffer);      // write something to the internal memory
    u8g2R->sendBuffer();               // transfer internal memory to the display
}
void requestData(WiFiClient* client) {
    const char* host = "www.linzag.at";
    Serial.printf("\n[Connecting to %s ... ", host);
    if (client->connect(host, 80)) {
        Serial.println("connected]");
        Serial.println("[Sending a request]");
        client->print(String("GET /static/XML_DM_REQUEST?name_dm=60500980&type_dm=any&mode=direct&limit=") + NO_TRIPS + " HTTP/1.1\r\n" +
                      "Host: " + host + "\r\n" +
                      "Connection: close\r\n" +
                      "\r\n");

        Serial.println("[Response:]");
    } else {
        Serial.println("[Connection failed!]");
        client->stop();
    }
}

void processResponse(int* state, String* line, int* i) {
    int idx = 0;
    int idx2 = 0;

    String subs;

    switch (*state) {  // state machine for reading input data
        case 0:        // read countdown = minutes to departure
            // find occurence of first "countdown=\"" in string
            idx = line->indexOf("countdown=\"");

            if (idx > 0) {
                idx2 = line->indexOf("\"", idx + 11);
                subs = line->substring(idx + 11, idx2);
                countdown[*i] = subs.toInt();  // convert the string we found to integer
                *state = 1;
            }
            break;
        case 1:  // read number = line number && direction = destination name
            idx = line->indexOf("number=\"");
            if (idx > 0) {
                idx2 = line->indexOf("\"", idx + 8);
                subs = line->substring(idx + 8, idx2);

                bimline[*i] = subs.toInt();  // convert the line number to integer

                idx = line->indexOf("direction=\"", idx2);

                idx2 = line->indexOf("\"", idx + 11);
                dest[*i] = line->substring(idx + 11, idx2);

                if (dest[*i].startsWith("JKU")) {
                    // if "JKU[...]", convert to "JKU Uni", otherwise it writes "JKU I U" onto the display
                    dest[*i].clear();
                    dest[*i] = "JKU Univ";
                }

                // print information on serial port -- debugging
                char string[70];
                dest[*i].toCharArray(string, dest[*i].length());
                Serial.printf("%02d: %02d %s %02d\n", *i, bimline[*i], string, countdown[*i]);

                // only if the state 1 was reached, a valid entry has been processed
                // therefore we can now increment our counter
                (*i)++;  // increment departure counter

                // switch to other state
                *state = 0;
            }
            break;
        default:
            *state = 0;
            break;
    }
}

void printResultsLED() {
    // write grabbed information onto the display
    char buffer[16];
    char string[7];
    int minutes;

    display_page++;  // show next display page

    // show different pages of the departure monitor
    // adds functionality and more movement
    int i = 3 * ((display_page - 1) % 4);
    for (int line = 0; line < 4; line++) {
        minutes = countdown[i];
        if (minutes > 99) {
            minutes = 99;  // only two digits are available
        }
        dest[i].toCharArray(string, dest[i].length());
        sprintf(buffer, "%02d %.*s %02d", bimline[i], 7, string, minutes);
        writeToDisplay(&(u8g2Lines[line]), buffer);

        // if (bimline[i] == 38 && dest[i].startsWith("J")) {
        //     line--;  // if it is "Jäger im Tal" we do not need to display
        // }
        i++;
    }
}
