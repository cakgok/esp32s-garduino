#ifndef ESP32_WEB_SERVER_H
#define ESP32_WEB_SERVER_H

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "ESPLogger.h"
#include "SensorManager.h"
#include "ConfigManager.h"

extern SensorManager sensorManager;

template<size_t BUFFER_SIZE>
class ESP32WebServer {
private:
    AsyncWebServer server;
    ESPLogger<BUFFER_SIZE>& logger;
    int serverPort;

    void setupRoutes() {
        // Serve static files
        server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

        // API endpoints
        server.on("/api/logs", HTTP_GET, std::bind(&ESP32WebServer::handleGetLogs, this, std::placeholders::_1));
        server.on("/api/config", HTTP_GET, std::bind(&ESP32WebServer::handleGetConfig, this, std::placeholders::_1));
        server.on("/api/config", HTTP_POST, std::bind(&ESP32WebServer::handlePostConfig, this, std::placeholders::_1));
        server.on("/api/sensorData", HTTP_GET, std::bind(&ESP32WebServer::handleGetSensorData, this, std::placeholders::_1));
        server.on("/api/relay", HTTP_POST, std::bind(&ESP32WebServer::handlePostRelay, this, std::placeholders::_1));
    }

    void handleGetLogs(AsyncWebServerRequest *request) {
        request->send(200, "text/html", logger.getLogsHTML());
    }

    void handleGetConfig(AsyncWebServerRequest *request) {
        request->send(200, "application/json", sensorManager.getConfigJSON());
    }

    void handlePostConfig(AsyncWebServerRequest *request) {
        if (request->hasParam("temperatureOffset", true)) {
            float tempOffset = request->getParam("temperatureOffset", true)->value().toFloat();
            sensorManager.setTemperatureOffset(tempOffset);
            logger.log("Temperature offset updated: %.2f C", tempOffset);
        }
        request->send(200);
    }

    void handleGetSensorData(AsyncWebServerRequest *request) {
        request->send(200, "application/json", sensorManager.getSensorData());
    }

    void handlePostRelay(AsyncWebServerRequest *request) {
        if (request->hasParam("relay", true) && request->hasParam("state", true)) {
            int relay = request->getParam("relay", true)->value().toInt();
            bool state = request->getParam("state", true)->value() == "true";
            
            if (setRelayState(relay, state)) {
                logger.log("Relay %d set to %s", relay, state ? "ON" : "OFF");
            }
        }
        request->send(200);
    }

public:
    ESP32WebServer(int port, ESPLogger<BUFFER_SIZE>& logger)
        : server(port), logger(logger), serverPort(port) {
        setupRoutes();
    }

    void begin() {
        server.begin();
        logger.log("Async HTTP server started on port %d", serverPort);
    }
};

#endif // ESP32_WEB_SERVER_H