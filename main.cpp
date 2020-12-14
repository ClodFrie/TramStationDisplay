
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

// function prototypes
int boyer_moore_search(char*, int, char*, int);

U8G2_MAX7219_64X8_F_4W_SW_SPI u8g2R0(U8G2_R0, /* clock=*/16, /* data=*/5, /* cs=*/4, /* dc=*/U8X8_PIN_NONE, /* reset=*/U8X8_PIN_NONE);
U8G2_MAX7219_64X8_F_4W_SW_SPI u8g2R1(U8G2_R0, /* clock=*/16, /* data=*/5, /* cs=*/0, /* dc=*/U8X8_PIN_NONE, /* reset=*/U8X8_PIN_NONE);
U8G2_MAX7219_64X8_F_4W_SW_SPI u8g2R2(U8G2_R0, /* clock=*/16, /* data=*/5, /* cs=*/14, /* dc=*/U8X8_PIN_NONE, /* reset=*/U8X8_PIN_NONE);
U8G2_MAX7219_64X8_F_4W_SW_SPI u8g2R3(U8G2_R0, /* clock=*/16, /* data=*/5, /* cs=*/12, /* dc=*/U8X8_PIN_NONE, /* reset=*/U8X8_PIN_NONE);
// U8G2_MAX7219_16X16_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ 11, /* data=*/ 12, /* cs=*/ 10, /* dc=*/ U8X8_PIN_NONE, /* reset=*/ U8X8_PIN_NONE);

WiFiClient client;
char receive[256];  // 0..255
char flag = 0;

void setup(void) {
    Serial.begin(115200);

    const char ssid[] = XXXXX;
    const char pass[] = XXXXX;
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

    u8g2R0.begin();
    u8g2R0.setContrast(4);
    u8g2R1.begin();
    u8g2R1.setContrast(4);
    u8g2R2.begin();
    u8g2R2.setContrast(4);
    u8g2R3.begin();
    u8g2R3.setContrast(4);
}

void loop(void) {
    const char* host = "www.linzag.at";

    WiFiClient client;
    // allocate trip variables
    int countdown[6];
    int bimline[6];
    String dest[6];

    Serial.printf("\n[Connecting to %s ... ", host);
    if (client.connect(host, 80)) {
        Serial.println("connected]");

        Serial.println("[Sending a request]");
        client.print(String("GET /static/XML_DM_REQUEST?name_dm=60500980&type_dm=any&mode=direct&limit=6") + " HTTP/1.1\r\n" +
                     "Host: " + host + "\r\n" +
                     "Connection: close\r\n" +
                     "\r\n");

        Serial.println("[Response:]");
        int i = 0;
        bool thereIsMore = false;
        while (client.connected() || client.available()) {
            if (client.available()) {
                String line = client.readStringUntil('\r');
                int idx, idx2;
                idx = line.indexOf("countdown=\"");
                while (idx > 0 || thereIsMore) {
                    // idx = line.indexOf("countdown=\"");
                    idx2 = line.indexOf("\"", idx + 11);
                    String subs = line.substring(idx + 11, idx2);

                    countdown[i] = subs.toInt();

                    idx = line.indexOf("number=\"", idx2);
                    idx2 = line.indexOf("\"", idx + 8);
                    subs = line.substring(idx + 8, idx2);

                    bimline[i] = subs.toInt();

                    idx = line.indexOf("direction=\"", idx2);
                    idx2 = line.indexOf("\"", idx + 11);
                    dest[i] = line.substring(idx + 11, idx2);

                    Serial.printf("ctd:%d,line:%d,direction:", countdown[i], bimline[i]);
                    Serial.println(dest[i] + "\n");
                    i++;

                    idx = line.indexOf("countdown=\"", idx2);
                    if (idx > 0) {
                        thereIsMore = true;
                    } else {
                        thereIsMore = false;
                    }
                }
            }
        }

        client.stop();
        Serial.println("\n[Disconnected]");
    } else {
        Serial.println("connection failed!]");
        client.stop();
    }
    

    for (int i = 0; Serial.available() && i < 256; i++) {
        if (i == 0) {
            memset(&receive, 0, sizeof(receive));
        }
        receive[i] = Serial.read();
    }
    char buffer[16];
    char string[4];
    int hours, minutes;

    minutes = countdown[0] % 60;
    hours = countdown[0] / 60;
    dest[0].toCharArray(string, dest[0].length());
    sprintf(buffer, "%02d %.*s %02d:%02d", bimline[0], 4, string, hours, minutes);
    u8g2R0.clearBuffer();              // clear the internal memory
    u8g2R0.setFont(u8g2_font_5x7_mf);  // choose a suitable font
    u8g2R0.drawStr(0, 7, buffer);      // write something to the internal memory
    u8g2R0.sendBuffer();               // transfer internal memory to the display

    minutes = countdown[1] % 60;
    hours = countdown[1] / 60;
    dest[1].toCharArray(string, dest[1].length());
    sprintf(buffer, "%02d %.*s %02d:%02d", bimline[1], 4, string, hours, minutes);
    u8g2R1.clearBuffer();                   // clear the internal memory
    u8g2R1.setFont(u8g2_font_5x7_mf);       // choose a suitable font
    u8g2R1.drawStr(0, 7, buffer);  // write something to the internal memory
    u8g2R1.sendBuffer();                    // transfer internal memory to the display

    minutes = countdown[2] % 60;
    hours = countdown[2] / 60;
    dest[2].toCharArray(string, dest[2].length());
    sprintf(buffer, "%02d %.*s %02d:%02d", bimline[2], 4, string, hours, minutes);
    u8g2R2.clearBuffer();                   // clear the internal memory
    u8g2R2.setFont(u8g2_font_5x7_mf);       // choose a suitable font
    u8g2R2.drawStr(0, 7, buffer);  // write something to the internal memory
    u8g2R2.sendBuffer();                    // transfer internal memory to the display

    minutes = countdown[3] % 60;
    hours = countdown[3] / 60;
    dest[3].toCharArray(string, dest[3].length());
    sprintf(buffer, "%02d %.*s %02d:%02d", bimline[3], 4, string, hours, minutes);
    u8g2R3.clearBuffer();                   // clear the internal memory
    u8g2R3.setFont(u8g2_font_5x7_mf);       // choose a suitable font
    u8g2R3.drawStr(0, 7, buffer);  // write something to the internal memory
    u8g2R3.sendBuffer();                    // transfer internal memory to the display

    delay(10000);
}
int boyer_moore_search(char* text, int textLen, char* search, int searchLen) {
    // preprocess Bad Character Rule lookup table
    // char lut[256][256];

    // loop over all positions in text
    for (int i = searchLen - 1; i < textLen; i++) {
        if (text[i] == '\0') {
            return -1;  // text did not contain search
        }
        // slide search mask over text
        for (int k = 0; k < searchLen; k++) {
            // printf("%d:%c\n", i - k, text[i - k]);

            // compare text with mask
            if (text[i - k] == search[searchLen - 1 - k]) {
                if (k == searchLen) {
                    // return position of first search result
                    return i - searchLen - 1;
                }
            } else {
                // if not matching, skip a few bytes (Bad Character Rule)

                i += searchLen - 1;
                break;
            }
        }
    }
    return -1;  // text did not contain search
}
