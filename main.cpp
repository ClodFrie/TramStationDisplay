
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

U8G2_MAX7219_64X8_F_4W_SW_SPI u8g2R0(U8G2_R0, /* clock=*/16, /* data=*/5, /* cs=*/4, /* dc=*/U8X8_PIN_NONE, /* reset=*/U8X8_PIN_NONE);
U8G2_MAX7219_64X8_F_4W_SW_SPI u8g2R1(U8G2_R0, /* clock=*/16, /* data=*/5, /* cs=*/0, /* dc=*/U8X8_PIN_NONE, /* reset=*/U8X8_PIN_NONE);
U8G2_MAX7219_64X8_F_4W_SW_SPI u8g2R2(U8G2_R0, /* clock=*/16, /* data=*/5, /* cs=*/14, /* dc=*/U8X8_PIN_NONE, /* reset=*/U8X8_PIN_NONE);
U8G2_MAX7219_64X8_F_4W_SW_SPI u8g2R3(U8G2_R0, /* clock=*/16, /* data=*/5, /* cs=*/12, /* dc=*/U8X8_PIN_NONE, /* reset=*/U8X8_PIN_NONE);
// U8G2_MAX7219_16X16_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ 11, /* data=*/ 12, /* cs=*/ 10, /* dc=*/ U8X8_PIN_NONE, /* reset=*/ U8X8_PIN_NONE);

U8G2_MAX7219_64X8_F_4W_SW_SPI u8g2Lines[4] = {u8g2R0, u8g2R1, u8g2R2, u8g2R3};

WiFiClient client;
char receive[256];  // 0..255
char flag = 0;
unsigned long act_time = 0;  // timeout variable

void setup(void) {
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
        (u8g2Lines[i]).setContrast(15);
    }

    // initialize timing variable
    act_time = millis();
}

void loop(void) {
    if (millis() > act_time + (unsigned long)15000) {
        const char* host = "www.linzag.at";
        act_time = millis();
        WiFiClient client;
        // allocate trip variables
        int noTrips = 10;  // number of trips to request save
        int countdown[noTrips];
        int bimline[noTrips];
        String dest[noTrips];

        Serial.printf("\n[Connecting to %s ... ", host);
        if (client.connect(host, 80)) {
            Serial.println("connected]");

            Serial.println("[Sending a request]");
            client.print(String("GET /static/XML_DM_REQUEST?name_dm=60500980&type_dm=any&mode=direct&limit=7") + " HTTP/1.1\r\n" +
                         "Host: " + host + "\r\n" +
                         "Connection: close\r\n" +
                         "\r\n");

            Serial.println("[Response:]");
            int i = 0;
            bool thereIsMore = true;
            while (client.connected()) {
                if (client.available()) {
                    // read string from website
                    // and grab departure information

                    String line = client.readStringUntil('\r');
                    int idx, idx2;
                    // find occurence of first "countdown=\"" in string
                    idx = line.indexOf("countdown=\"");

                    while (idx > 0 && thereIsMore) {
                        // idx = line.indexOf("countdown=\"");  // commented out -> this one is done prior to the loop
                        idx2 = line.indexOf("\"", idx + 11);
                        String subs = line.substring(idx + 11, idx2);

                        countdown[i] = subs.toInt();  // convert the string we found to integer

                        idx = line.indexOf("number=\"", idx2);
                        idx2 = line.indexOf("\"", idx + 8);
                        subs = line.substring(idx + 8, idx2);

                        bimline[i] = subs.toInt();  // convert the line number to integer

                        idx = line.indexOf("direction=\"", idx2);
                        idx2 = line.indexOf("\"", idx + 16);
                        dest[i] = line.substring(idx + 16, idx2);
                        if (dest[i].startsWith("JKU")) {
                            // if "JKU[...]", convert to "JKU Universität", otherwise it writes "JKU I U" onto the display
                            dest[i] = "JKU Universität";
                        } else if (dest[i].startsWith("J")) {  // disregard Jäger im Tal as valid destination
                            continue;
                        }

                        Serial.printf("ctd:%d,line:%d,direction:", countdown[i], bimline[i]);
                        Serial.println(dest[i]);
                        i++;

                        idx = line.indexOf("countdown=\"", idx2);
                        if (idx > 0 && i < 4) {
                            thereIsMore = true;
                            yield();  // let ESP do some background stuff e.g. networking
                        } else {
                            thereIsMore = false;
                        }
                    }
                    yield();
                }
            }
            // stopping client connection
            client.stop();
            Serial.println("\n[Disconnected]");

            // write grabbed information onto the display
            char buffer[16];
            char string[7];
            int minutes;

            for (int i = 0; i < 4; i++) {
                minutes = countdown[i];
                if (minutes > 99) {
                    minutes = 99;  // only two digits are available
                }
                dest[i].toCharArray(string, dest[i].length());
                sprintf(buffer, "%02d %.*s %02d", bimline[i], 7, string, minutes);
                writeToDisplay(&(u8g2Lines[i]), buffer);
            }
        }

        else {
            Serial.println("connection failed!]");
            client.stop();
        }
    } else {
        // would do it anyway, but just to be sure ;)
        yield();
    }
}

void writeToDisplay(U8G2_MAX7219_64X8_F_4W_SW_SPI* u8g2R, char* buffer) {
    // writes buffer to one line of a matrix display
    u8g2R->clearBuffer();              // clear the internal memory
    u8g2R->setFont(u8g2_font_5x7_mf);  // choose a suitable font
    u8g2R->drawStr(0, 7, buffer);      // write something to the internal memory
    u8g2R->sendBuffer();               // transfer internal memory to the display
}
