#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include "secrets.h"

#define SMOKE_PIN 4

static bool wifiActive = false;

static bool serverActive = false;
static ESP8266WebServer srv(80);

static bool smokeActive = false;
static unsigned long offTimestamp = 0;

void wifiSetup()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void wifiTick()
{
    bool newWifiActive = WiFi.status() == WL_CONNECTED;
    if (newWifiActive != wifiActive) {
        if (newWifiActive) {
            // wifi has connected!
            Serial.print("WiFi connected, IP: ");
            Serial.println(WiFi.localIP());
        } else {
            // wifi has lost connection!
            Serial.println("WiFi disconnected, reconnecting!");
            WiFi.mode(WIFI_STA);
            WiFi.begin(WIFI_SSID, WIFI_PASS);
        }
        wifiActive = newWifiActive;
    }
}

void handleRoot()
{
    srv.send(200, "text/html", R"HTML(
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

        button, input {
            margin: 5px;
            padding: 5px;
            background: orange;
            font-size: 2em;
        }
                </style>
                <script type="text/javascript">
                    function activateSmoke() {
                        var duration = document.getElementById("duration").value;
                        fetch(`/activate?duration=${duration}`, {method: "POST"});
                    }
                    function deactivateSmoke() {
                        fetch("/deactivate", {method: "POST"});
                    }
                </script>
            </head>
            <button onclick="activateSmoke()">Activate Smoke</button>
            <input type="number" id="duration" min="1" max="90">
            <button onclick="deactivateSmoke()">Deactivate Smoke</button>
        </html>
        )HTML");
}

void handleActivate()
{
    if (smokeActive) {
        srv.send(409, "text/plain", "smoke already active");
    } else {
        const auto durationArg = srv.arg("duration");
        unsigned long duration = durationArg.toInt();
        if (duration < 1 || duration > 90) {
            // default duration to 30 seconds if it is outside of 1 to 90 seconds
            duration = 30;
        }
        duration *= 1000;
        offTimestamp = millis() + duration;
        smokeActive = true;
        digitalWrite(SMOKE_PIN, HIGH);
        srv.send(200, "text/plain", "activated");
    }
}

void handleDeactivate()
{
    offTimestamp = 0;
    smokeActive = false;
    digitalWrite(SMOKE_PIN, LOW);
    srv.send(200, "text/plain", "deactivate");
}

void serverSetup()
{
    pinMode(SMOKE_PIN, OUTPUT);
    digitalWrite(SMOKE_PIN, LOW);

    srv.on("/", handleRoot);
    srv.on("/activate", handleActivate);
    srv.on("/deactivate", handleDeactivate);
}

void serverTick()
{
    if (wifiActive && !serverActive) {
        // activate server when wifi activates
        srv.begin();
        serverActive = true;
    }

    if (smokeActive) {
        unsigned long now = millis();
        if (now >= offTimestamp) {
            offTimestamp = 0;
            smokeActive = false;
            digitalWrite(SMOKE_PIN, LOW);
        }
    }

    if (serverActive) {
        srv.handleClient();
    }
}

void setup()
{
    Serial.begin(74880); // NOTE: matches boot loader for convenience
    Serial.println();
    Serial.println("****INIT****"); // NOTE: clear line after boot loader

    // setup and start wifi connection
    Serial.println("Initiating WiFi connection...");
    wifiSetup();

    // setup webserver
    serverSetup();
}

void loop()
{
    wifiTick();
    serverTick();
}
