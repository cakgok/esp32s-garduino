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
    int serverPort;
    Logger& logger;
    RelayManager& relayManager;
    SensorManager& sensorManager; 
    ConfigManager& configManager; 
    AsyncEventSource* events;
    WebSocketManager wsManager;
    JsonHandler jsonHandler;

    void setupRoutes() {
        server.on("/favicon.ico", HTTP_GET, [this](AsyncWebServerRequest *request){
            if (LittleFS.exists("/favicon.ico")) {
                request->send(LittleFS, "/favicon.ico", "image/x-icon");
            } else {
                request->send(204);
            }
        });

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

        // Catch-all handler for /api/ routes
        server.on("/api/", HTTP_ANY, [](AsyncWebServerRequest *request){
            request->send(404, "text/plain", "API endpoint not found");
        });

        // Serve static files last
        server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
        server.serveStatic("/index.css", LittleFS, "/index.css");
        server.serveStatic("/index.js", LittleFS, "/index.js");

        server.serveStatic("/config.html", LittleFS, "/config.html");
        server.serveStatic("/config.css", LittleFS, "/config.css");
        server.serveStatic("/config.js", LittleFS, "/config.js");

        server.serveStatic("/logs.html", LittleFS, "/logs.html");
        server.serveStatic("/logs.css", LittleFS, "/logs.css");
        server.serveStatic("/logs.js", LittleFS, "/logs.js");
    }

    void handleGetLogs(AsyncWebServerRequest *request) {
        static size_t currentOffset = 0;
        size_t totalLogs = Logger::instance().getLogCount();

        if (currentOffset < totalLogs) {
            String logJson = Logger::instance().peekNextLogJson(currentOffset);
            if (logJson.length() > 0) {
                request->send(200, "application/json", logJson);
                currentOffset++;
            } else {
                // This case shouldn't happen if peekNextLogJson is implemented correctly,
                // but we'll handle it just in case
                request->send(204); // No Content
                currentOffset = totalLogs; // Force exit condition
            }
        } else {
            request->send(204); // No Content
            currentOffset = 0; // Reset offset when we've gone through all logs
        }
    }

    void handleGetConfig(AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc = JsonHandler::createConfigJson(configManager);
        serializeJson(doc, *response);
        request->send(response);
    }

    void handlePostConfig(AsyncWebServerRequest *request) {
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
        logger.log("WebServer", Logger::Level::INFO, "Sending sensor data to client");
        request->send(response);
    }

    void handlePostRelay(AsyncWebServerRequest *request, JsonVariant &json) {
        if (json.is<JsonObject>()) {
            JsonObject jsonObj = json.as<JsonObject>();
            
            if (jsonObj.containsKey("relay") && jsonObj.containsKey("active")) {
                int relayIndex = jsonObj["relay"].as<int>();
                bool active = jsonObj["active"].as<bool>();
                
                if (relayIndex >= 0 && relayIndex < ConfigConstants::RELAY_COUNT) {
                    bool success = active ? relayManager.activateRelay(relayIndex) : relayManager.deactivateRelay(relayIndex);
                    
                    if (success) {
                        String response = "{\"success\":true,\"relayIndex\":" + String(relayIndex) + 
                                        ",\"message\":\"Relay " + String(active ? "activated" : "deactivated") + "\"}";
                        request->send(200, "application/json", response);
                    } else {
                        request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to toggle relay\"}");
                    }
                } else {
                    request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid relay index\"}");
                }
            } else {
                request->send(400, "application/json", "{\"success\":false,\"message\":\"Missing relay or active parameter\"}");
            }
        } else {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON format\"}");
        }
    }

    void handleResetToDefault(AsyncWebServerRequest *request) {
        configManager.resetToDefault();
        request->send(200, "text/plain", "Configuration reset to default");
    }
      
public:
    ESP32WebServer(int port, RelayManager& relayManager, SensorManager& sensorManager, ConfigManager& configManager)
        :   server(port), 
            logger(Logger::instance()),
            relayManager(relayManager), 
            serverPort(port), 
            sensorManager(sensorManager), 
            configManager(configManager),
            wsManager(server) 
        {
            setupRoutes();
            logger.addLogObserver([this](std::string_view tag, Logger::Level level, std::string_view message) {
                this->handleLogMessage(tag, level, message);
            });
            relayManager.setNotifyClientsCallback([this]() { this->notifyClients(); });
        }

    void begin() {
        server.begin();
        logger.log("WebServer", Logger::Level::INFO, "Async HTTP server started on port {} with WebSocket support", serverPort);
    }

    void handleLogMessage(std::string_view tag, Logger::Level level, std::string_view message) {
        wsManager.handleLog(tag, level, message);
    }

    void sendUpdate() {
        logger.log("WebServer", Logger::Level::DEBUG, "sendUpdate() called");
        JsonDocument doc = jsonHandler.createSensorDataJson(sensorManager, relayManager, configManager);
        String output;
        serializeJson(doc, output);
        logger.log("WebServer", Logger::Level::DEBUG, "Sending SSE update: %s", output.c_str());
        events->send(output.c_str(), "update", millis());
    }

    // Call this method whenever sensor data or relay states change
    void notifyClients() {
        logger.log("WebServer", Logger::Level::DEBUG, "notifyClients() called");
        sendUpdate();
    }
};

#endif // ESP32_WEB_SERVER_H