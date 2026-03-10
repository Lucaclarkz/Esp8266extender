#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

extern "C" {
#include "user_interface.h"
#include "wpa2_enterprise.h"
}

AsyncWebServer server(80);
DynamicJsonDocument Config(2048);

bool RepeaterIsWorking = true;
unsigned long previousMillis = 0;
long delay_time = 0;
int ledState = LOW;

class wifi_ext {
public:
    void create_server() {
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            String network_html = "";
            int n = WiFi.scanComplete();
            if (n == -2) { WiFi.scanNetworks(true); }
            else if (n) {
                for (int i = 0; i < n; ++i) {
                    String router = WiFi.SSID(i);
                    network_html += "<input type=\"radio\" name=\"ssid\" value=\"" + router + "\" required><label>" + router + "</label><br>";
                }
                WiFi.scanDelete();
            }

            String html = "<html><head><style>body{font-family:sans-serif;padding:20px;} input{margin-bottom:10px;}</style></head><body>";
            html += "<h1>WiFi Extender (Enterprise Support)</h1>";
            html += "<form action=\"/credentials\">";
            html += "<h3>Select Network:</h3>" + network_html;
            html += "<br><input type=\"text\" name=\"user\" placeholder=\"Username (For Myanmar Net)\"><br>";
            html += "<input type=\"password\" name=\"pass\" placeholder=\"Password\" required><br>";
            html += "<input type=\"text\" name=\"ap\" placeholder=\"New Hotspot Name\" required><br>";
            html += "<input type=\"submit\" value=\"Save and Restart\">";
            html += "</form></body></html>";
            request->send(200, "text/html", html);
        });

        server.on("/credentials", HTTP_GET, [](AsyncWebServerRequest *request) {
            if (request->hasParam("ssid")) Config["ssid"] = request->getParam("ssid")->value();
            if (request->hasParam("user")) Config["user"] = request->getParam("user")->value();
            if (request->hasParam("pass")) Config["pass"] = request->getParam("pass")->value();
            if (request->hasParam("ap")) Config["ap"] = request->getParam("ap")->value();

            String output;
            serializeJson(Config, output);
            File file = LittleFS.open("/config.json", "w");
            file.print(output);
            file.close();
            request->send(200, "text/plain", "Config Saved. Restarting...");
            delay(2000);
            ESP.restart();
        });
    }

    String get_credentials(int a) {
        File file = LittleFS.open("/config.json", "r");
        if (!file) return "null";
        String content = file.readString();
        deserializeJson(Config, content);
        file.close();
        String creds[] = {Config["ssid"], Config["pass"], Config["ap"], Config["user"]};
        return creds[a];
    }
};

wifi_ext my_wifi;

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    LittleFS.begin();

    String ssid = my_wifi.get_credentials(0);
    String pass = my_wifi.get_credentials(1);
    String ap = my_wifi.get_credentials(2);
    String user = my_wifi.get_credentials(3);

    if (ssid == "null") {
        WiFi.softAP("Pius_Extender_Setup");
        my_wifi.create_server();
        server.begin();
        delay_time = 1000;
    } else {
        WiFi.mode(WIFI_STA);
        if (user != "" && user != "null") {
            // Enterprise Connection Logic
            struct station_config sta_conf;
            wifi_station_get_config(&sta_conf);
            os_memset(sta_conf.ssid, 0, 32);
            os_memcpy(sta_conf.ssid, ssid.c_str(), ssid.length());
            wifi_station_set_config(&sta_conf);
            wifi_station_set_wpa2_enterprise_auth(1);
            wifi_station_set_enterprise_username((uint8 *)user.c_str(), user.length());
            wifi_station_set_enterprise_password((uint8 *)pass.c_str(), pass.length());
            wifi_station_connect();
        } else {
            WiFi.begin(ssid, pass);
        }

        while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
        
        // NAPT Enable
        ip_napt_init(1000, 10);
        ip_napt_enable_no(SOFTAP_IF, 1);
        WiFi.softAP(ap, pass);
        delay_time = 200;
    }
}

void loop() {
    // Led blink logic
    if (millis() - previousMillis >= delay_time) {
        previousMillis = millis();
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
    }
}
