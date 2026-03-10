#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>

#define NAPT 1000
#define NAPT_PORT 10

extern "C" {
#include "user_interface.h"
#include "wpa2_enterprise.h"
#include "lwip/napt.h"
}

AsyncWebServer server(80);

unsigned long previousMillis = 0;
long delay_time = 500; 
int ledState = LOW;

class wifi_ext {
public:
    String ssid = "";
    String pass = "";
    String ap   = "";
    String user = "";

    void create_server() {
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            String network_html = "";
            int n = WiFi.scanComplete();

            // ChatGPT အကြံပြုချက် - Scanning status ပြခြင်း
            if (n == -2) {
                WiFi.scanNetworks(true);
                network_html = "<p style='color:blue;'>Scanning WiFi... refresh in a few seconds.</p>";
            } else if (n == 0) {
                network_html = "<p style='color:red;'>No WiFi found. Refresh again.</p>";
            } else if (n > 0) {
                for (int i = 0; i < n; ++i) {
                    String router = WiFi.SSID(i);
                    network_html += "<input type=\"radio\" name=\"ssid\" value=\"" + router + "\" required><label>" + router + "</label><br>";
                }
                WiFi.scanDelete();
            }

            String html = "<html><head><title>Pius Extender</title><style>body{font-family:sans-serif;padding:20px;} input{margin-bottom:15px; width:100%;}</style></head><body>";
            html += "<h1>WiFi Extender Config</h1><form action=\"/credentials\">";
            html += "<h3>1. Select WiFi:</h3>" + network_html;
            html += "<h3>2. Enterprise User (Identity):</h3><input type=\"text\" name=\"user\">";
            html += "<h3>3. WiFi Password:</h3><input type=\"password\" name=\"pass\" required>";
            html += "<h3>4. New AP Name:</h3><input type=\"text\" name=\"ap\">";
            html += "<input type=\"submit\" value=\"Save and Restart\"></form></body></html>";
            request->send(200, "text/html", html);
        });

        server.on("/credentials", HTTP_GET, [](AsyncWebServerRequest *request) {
            File file = LittleFS.open("/config.txt", "w");
            if (file) {
                file.println(request->hasParam("ssid") ? request->getParam("ssid")->value() : "");
                file.println(request->hasParam("pass") ? request->getParam("pass")->value() : "");
                file.println(request->hasParam("ap") ? request->getParam("ap")->value() : "");
                file.println(request->hasParam("user") ? request->getParam("user")->value() : "");
                file.close();
            }
            request->send(200, "text/plain", "Saved. Restarting...");
            delay(2000); ESP.restart();
        });
    }

    bool load_credentials() {
        File file = LittleFS.open("/config.txt", "r");
        if (!file) return false;
        ssid = file.readStringUntil('\n'); ssid.trim();
        pass = file.readStringUntil('\n'); pass.trim();
        ap = file.readStringUntil('\n'); ap.trim();
        user = file.readStringUntil('\n'); user.trim();
        file.close();
        return ssid.length() > 0;
    }
};

wifi_ext my_wifi;

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    // ChatGPT အကြံပြုချက် - LittleFS mount စစ်ဆေးခြင်း
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        return; 
    }

    if (!my_wifi.load_credentials()) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP("Pius_Extender_Setup");
        my_wifi.create_server();
        server.begin();
        delay_time = 1000;
    } else {
        WiFi.mode(WIFI_AP_STA);

        if (my_wifi.user != "") {
            wifi_station_disconnect();
            struct station_config sta_conf;
            memset(&sta_conf, 0, sizeof(sta_conf)); 

            size_t len = my_wifi.ssid.length();
            if (len > 31) len = 31;
            memcpy(sta_conf.ssid, my_wifi.ssid.c_str(), len);
            sta_conf.ssid[len] = 0;
            
            wifi_station_set_config(&sta_conf);
            wifi_station_set_wpa2_enterprise_auth(1);
            wifi_station_set_enterprise_username((uint8 *)my_wifi.user.c_str(), my_wifi.user.length());
            wifi_station_set_enterprise_password((uint8 *)my_wifi.pass.c_str(), my_wifi.pass.length());
            wifi_station_connect();
        } else {
            WiFi.begin(my_wifi.ssid.c_str(), my_wifi.pass.c_str());
        }

        int count = 0;
        while (WiFi.status() != WL_CONNECTED && count < 30) {
            delay(500); Serial.print("."); count++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            ip_napt_init(NAPT, NAPT_PORT);
            ip_napt_enable_no(SOFTAP_IF, 1);
            
            String ap_name = (my_wifi.ap.length() > 0) ? my_wifi.ap : "Pius_Extender";
            String ap_pass = (my_wifi.pass.length() >= 8) ? my_wifi.pass : "12345678";
            WiFi.softAP(ap_name.c_str(), ap_pass.c_str());
            
            delay_time = 200;
        } else {
            LittleFS.remove("/config.txt");
            ESP.restart();
        }
    }
}

void loop() {
    if (millis() - previousMillis >= (unsigned long)delay_time) {
        previousMillis = millis();
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
    }
}
