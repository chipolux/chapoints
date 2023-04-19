#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include "secrets.h"
#include "shock.h"

#define GPIO_2 2

ESP8266WebServer server(80);

void handleRoot()
{
    server.send(200, "text/html", R"HTML(
        <html>
            <head>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1">
                <style>
        html, body {
            margin: 10px;
            padding: 0;
        }

        body {
            display: flex;
            flex-direction: column;
        }

        button {
            margin: 5px;
            padding: 5px;
            background: orange;
            font-size: 2em;
        }
                </style>
                <script type="text/javascript">
                    function testFeature(feature) {
                        fetch(`/${feature}`, {method: "POST"});
                    }
                </script>
            </head>
            <button onclick="testFeature('shock')">Test Shock</button>
            <button onclick="testFeature('poweron')">Test Power On</button>
        </html>
        )HTML");
}

void sendMessage(const shock::MessageType &messageType)
{
    server.send(200, "text/plain", "message sent");
    shock::sendMessage(GPIO_2, messageType);
}

void setup()
{
    // setup pin for shock collar
    shock::setupPin(GPIO_2);

    // setup and start wifi connection
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    // setup webserver
    server.on("/", handleRoot);
    server.on("/shock", []() { sendMessage(shock::TEST_SHOCK); });
    server.on("/poweron", []() { sendMessage(shock::POWER_ON); });
    /* these don't do anything much so ignore them for now
    server.on("/mode0", []() { sendMessage(shock::MODE_0); });
    server.on("/mode1", []() { sendMessage(shock::MODE_1); });
    server.on("/mode2", []() { sendMessage(shock::MODE_2); });
    server.on("/mode3", []() { sendMessage(shock::MODE_3); });
    server.on("/mode4", []() { sendMessage(shock::MODE_4); });
    server.on("/mode5", []() { sendMessage(shock::MODE_5); });
    server.on("/poweroff", []() { sendMessage(shock::POWER_OFF); });
    */
    server.begin();
}

void loop() { server.handleClient(); }
