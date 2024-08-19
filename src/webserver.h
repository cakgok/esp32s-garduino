#ifndef ESP32_WEB_SERVER_H
#define ESP32_WEB_SERVER_H

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ESPLogger.h"
#include "SensorManager.h"
#include "ConfigManager.h"
#include "RelayManager.h"
#include "Globals.h"
#include <vector>
#include <AsyncJson.h>
#include "ESPLogger.h"
#include "WebsocketManager.h"
#include "JsonHandler.h"

class ESP32WebServer {
private:
    AsyncWebServer server;
    AsyncWebSocket ws;
    int serverPort;
    Logger& logger;
    RelayManager& relayManager;
    SensorManager& sensorManager; 
    ConfigManager& configManager; 
    AsyncEventSource* events;
    WebSocketManager wsManager;
    JsonHandler jsonHandler;

    void setupRoutes() {
        // Serve static files
        server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
        server.serveStatic("/index.css", LittleFS, "/index.css");
        server.serveStatic("/index.js", LittleFS, "/index.js");

        server.serveStatic("/config.html", LittleFS, "/config.html");
        server.serveStatic("/config.css", LittleFS, "/config.css");
        server.serveStatic("/config.js", LittleFS, "/config.js");

        server.serveStatic("/logs.html", LittleFS, "/logs.html");
        server.serveStatic("/logs.css", LittleFS, "/logs.css");
        server.serveStatic("/logs.js", LittleFS, "/logs.js");

        // API endpoints
        server.on("/api/logs", HTTP_GET, std::bind(&ESP32WebServer::handleGetLogs, this, std::placeholders::_1));
        server.on("/api/config", HTTP_GET, std::bind(&ESP32WebServer::handleGetConfig, this, std::placeholders::_1));
        server.on("/api/config", HTTP_POST, std::bind(&ESP32WebServer::handlePostConfig, this, std::placeholders::_1));
        server.on("/api/sensorData", HTTP_GET, std::bind(&ESP32WebServer::handleGetSensorData, this, std::placeholders::_1));
        server.on("/api/resetToDefault", HTTP_GET, std::bind(&ESP32WebServer::handleResetToDefault, this, std::placeholders::_1));

        // Add SSE route
        events = new AsyncEventSource("/api/events");
        server.addHandler(events);

        // Async JSON
        AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/api/relay", std::bind(&ESP32WebServer::handlePostRelay, this, std::placeholders::_1, std::placeholders::_2));
        server.addHandler(handler);
    }

    void handleGetLogs(AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;
        JsonArray logsArray = doc["logs"].to<JsonArray>();

        auto logs = logger.getChronologicalLogs();
        for (const auto& log : logs) {
            JsonObject logObj = logsArray.add<JsonObject>();
            logObj["tag"] = log.tag;
            logObj["level"] = static_cast<int>(log.level);
            logObj["message"] = log.message;
        }

        serializeJson(doc, *response);
        request->send(response);
    }

    void handleGetConfig(AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc = JsonHandler::createConfigJson(configManager);
        serializeJson(doc, *response);
        request->send(response);
    }

    void ESP32WebServer::handlePostConfig(AsyncWebServerRequest *request) {
        if (request->hasParam("config", true)) {
            String configJson = request->getParam("config", true)->value();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, configJson);

            if (error) {
                request->send(400, "text/plain", "Invalid JSON");
                return;
            }

            if (JsonHandler::updateConfig(configManager, doc)) {
                request->send(200, "text/plain", "Configuration updated");
            } else {
                request->send(400, "text/plain", "Failed to update configuration");
            }
        } else {
            request->send(400, "text/plain", "Missing config parameter");
        }
    }

    void handleGetSensorData(AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc = jsonHandler.createSensorDataJson(sensorManager, relayManager, configManager);
        serializeJson(doc, *response);
        logger.log(Logger::Level::INFO, "Sending sensor data to client");
        request->send(response);
    }

    void ESP32WebServer::handlePostRelay(AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        
        if (JsonHandler::handleRelayOperation(relayManager, jsonObj)) {
            String response = "{\"success\":true,\"relayIndex\":" + String(jsonObj["relay"].as<int>()) + 
                            ",\"message\":\"Relay " + String(jsonObj["active"].as<bool>() ? "activated" : "deactivated") + "\"}";
            request->send(200, "application/json", response);
        } else {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Failed to toggle relay\"}");
        }
    }

    void handleResetToDefault(AsyncWebServerRequest *request) {
        configManager.resetToDefault();
        request->send(200, "text/plain", "Configuration reset to default");
    }
      
public:
    ESP32WebServer(int port, RelayManager& relayManager, SensorManager& sensorManager, ConfigManager& configManager)
        : server(port), 
        logger(Logger::instance()),
        relayManager(relayManager), 
        serverPort(port), 
        sensorManager(sensorManager), 
        configManager(configManager),
        ws("/ws"), // Initialize the WebSocket with a path
        wsManager(server) {
        setupRoutes();
        logger.addLogObserver([this](std::string_view tag, Logger::Level level, std::string_view message) {
            this->handleLogMessage(tag, level, message);
        });
    }

    void begin() {
        server.begin();
        logger.log(Logger::Level::INFO, "Async HTTP server started on port {} with WebSocket support", serverPort);
    }

    void handleLogMessage(std::string_view tag, Logger::Level level, std::string_view message) {
        wsManager.handleLog(tag, level, message);
    }

    void sendUpdate() {
        JsonDocument doc = jsonHandler.createSensorDataJson(sensorManager, relayManager, configManager);
        String output;
        serializeJson(doc, output);
        events->send(output.c_str(), "update", millis());
    }

    // Call this method whenever sensor data or relay states change
    void notifyClients() {
        sendUpdate();
    }
};

#endif // ESP32_WEB_SERVER_H