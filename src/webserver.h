#ifndef ESP32_WEB_SERVER_H
#define ESP32_WEB_SERVER_H

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ESPLogger.h"
#include "SensorManager.h"
#include "ConfigManager.h"
#include "RelayManager.h"
#include "globals.h"
#include <vector>
#include <AsyncJson.h>
#include <esp_log.h>

class ESP32WebServer {
private:
    AsyncWebServer server;
    int serverPort;
    ESPLogger& logger;
    RelayManager& relayManager;
    SensorManager& sensorManager; 
    ConfigManager& configManager; 
    AsyncEventSource* events;
    // Add this at the top of your file, outside of any function
    String capturedBody = "";

    void setupRoutes() {
        // Serve static files
        server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
        server.serveStatic("/index.css", LittleFS, "/index.css");
        server.serveStatic("/index.js", LittleFS, "/index.js");

        server.serveStatic("/config.html", LittleFS, "/config.html");
        server.serveStatic("/config.css", LittleFS, "/config.css");
        server.serveStatic("/config.js", LittleFS, "/config.js");

        server.serveStatic("/log.html", LittleFS, "/log.html");
        server.serveStatic("/log.css", LittleFS, "/log.css");
        server.serveStatic("/log.js", LittleFS, "/log.js");

        // API endpoints
        server.on("/api/logs", HTTP_GET, std::bind(&ESP32WebServer::handleGetLogs, this, std::placeholders::_1));
        server.on("/api/config", HTTP_GET, std::bind(&ESP32WebServer::handleGetConfig, this, std::placeholders::_1));
        server.on("/api/config", HTTP_POST, std::bind(&ESP32WebServer::handlePostConfig, this, std::placeholders::_1));
        server.on("/api/sensorData", HTTP_GET, std::bind(&ESP32WebServer::handleGetSensorData, this, std::placeholders::_1));
        //server.on("/api/relay", HTTP_POST, std::bind(&ESP32WebServer::handlePostRelay, this, std::placeholders::_1));
        server.on("/api/resetToDefault", HTTP_GET, std::bind(&ESP32WebServer::handleGetDefaults, this, std::placeholders::_1));

        // Add SSE route
        events = new AsyncEventSource("/api/events");
        server.addHandler(events);

        //aysnc json
        AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/api/relay",std::bind(&ESP32WebServer::handlePostRelay, this, std::placeholders::_1, std::placeholders::_2));
        server.addHandler(handler);
    }

    void handleGetLogs(AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("text/plain");
        logger.processLogs([response](const String& logEntry) {
            response->println(logEntry);
        });
        request->send(response);
    }

    void handleGetConfig(AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;

        const ConfigManager::CachedConfig& config = configManager.getCachedConfig();

        doc["temperatureOffset"] = configManager.getTemperatureOffset();
        doc["telemetryInterval"] = configManager.getTelemetryInterval();
        doc["sensorUpdateInterval"] = configManager.getSensorUpdateInterval();
        doc["lcdUpdateInterval"] = configManager.getLCDUpdateInterval();
        doc["sensorPublishInterval"] = configManager.getSensorPublishInterval();
    
        JsonArray sensorConfigs = doc["sensorConfigs"].to<JsonArray>();
        for (size_t i = 0; i < RELAY_COUNT; i++) {
            JsonObject sensorConfig = sensorConfigs.add<JsonObject>();
            const auto& config = configManager.getSensorConfig(i);
            sensorConfig["threshold"] = config.threshold;
            sensorConfig["activationPeriod"] = config.activationPeriod;
            sensorConfig["wateringInterval"] = config.wateringInterval;
        }

        serializeJson(doc, *response);
        request->send(response);
    }

    void handlePostConfig(AsyncWebServerRequest *request) {
        if (request->hasParam("config", true)) {
            String configJson = request->getParam("config", true)->value();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, configJson);

            if (error) {
                logger.log(LogLevel::ERROR, "Failed to parse config JSON");
                request->send(400, "text/plain", "Invalid JSON");
                return;
            }

            ConfigManager::CachedConfig newConfig;

            // Parse global settings
            if (doc.containsKey("temperatureOffset")) newConfig.temperatureOffset = doc["temperatureOffset"].as<float>();
            if (doc.containsKey("telemetryInterval")) newConfig.telemetryInterval = doc["telemetryInterval"].as<uint32_t>();
            if (doc.containsKey("sensorUpdateInterval")) newConfig.sensorUpdateInterval = doc["sensorUpdateInterval"].as<uint32_t>();
            if (doc.containsKey("lcdUpdateInterval")) newConfig.lcdUpdateInterval = doc["lcdUpdateInterval"].as<uint32_t>();
            if (doc.containsKey("sensorPublishInterval")) newConfig.sensorPublishInterval = doc["sensorPublishInterval"].as<uint32_t>();

            // Parse sensor configs
            if (doc.containsKey("sensorConfigs")) {
                JsonArray sensorConfigs = doc["sensorConfigs"];
                for (size_t i = 0; i < std::min(sensorConfigs.size(), static_cast<size_t>(RELAY_COUNT)); i++) {
                    JsonObject sensorConfig = sensorConfigs[i];
                    newConfig.sensorConfigs[i] = ConfigManager::SensorConfig{
                        sensorConfig["threshold"].as<float>(),
                        sensorConfig["activationPeriod"].as<uint32_t>(),
                        sensorConfig["wateringInterval"].as<uint32_t>()
                    };
                }
            }

            bool success = configManager.updateAndSaveAll(
                newConfig.temperatureOffset,
                newConfig.telemetryInterval,
                newConfig.sensorUpdateInterval,
                newConfig.lcdUpdateInterval,
                newConfig.sensorPublishInterval,
                newConfig.sensorConfigs
            );

            if (success) {
                logger.log(LogLevel::INFO, "Configuration updated successfully");
                request->send(200, "text/plain", "Configuration updated");
            } else {
                logger.log(LogLevel::ERROR, "Failed to update configuration");
                request->send(500, "text/plain", "Failed to update configuration");
            }
        } else {
            request->send(400, "text/plain", "Missing config parameter");
        }
    }

    void handleGetSensorData(AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;
        
        // Get all sensor data using the existing function
        SensorData sensorData = sensorManager.getSensorData();

        // Populate doc with sensor data
        JsonArray plants = doc["plants"].to<JsonArray>();
        for (int i = 0; i < 4; i++) {
            JsonObject plant = plants.add<JsonObject>();
            plant["moisture"] = sensorData.moisture[i];
        }

        doc["temperature"] = sensorData.temperature;
        doc["pressure"] = sensorData.pressure;
        doc["waterLevel"] = sensorData.waterLevel;

         // Add relay states
        JsonArray relays = doc["relays"].to<JsonArray>();
        for (int i = 0; i < RELAY_COUNT; i++) {
            JsonObject relay = relays.add<JsonObject>();
            relay["active"] = relayManager.isRelayActive(i);
            if (relayManager.isRelayActive(i)) {
                relay["activationTime"] = configManager.getSensorConfig(i).activationPeriod;
            }
        }
        serializeJson(doc, *response);
        logger.log("Sending sensor data to client ");
        request->send(response);
    }

    void handlePostRelay(AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        
        if (jsonObj.containsKey("relay") && jsonObj.containsKey("active")) {
            int relayIndex = jsonObj["relay"];
            bool active = jsonObj["active"];

            if (relayIndex >= 0 && relayIndex < RELAY_COUNT) {
                bool success;
                if (active) {
                    success = relayManager.activateRelay(relayIndex);
                } else {
                    success = relayManager.deactivateRelay(relayIndex);
                }

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
    }

    void handleGetDefaults(AsyncWebServerRequest *request) {
        //get deafults from configManager.getDefaultConfig();
    }

public:
    ESP32WebServer(int port, ESPLogger& logger, RelayManager& relayManager, SensorManager& sensorManager, ConfigManager& configManager)
        : server(port), logger(logger), relayManager(relayManager), serverPort(port), sensorManager(sensorManager), configManager(configManager) {
        setupRoutes();
    }


    void begin() {
        server.begin();
        logger.log(LogLevel::INFO, "Async HTTP server started on port %d", serverPort);
    }

    void sendUpdate() {
        JsonDocument doc;
        // Get all sensor data using the existing function
        SensorData sensorData = sensorManager.getSensorData();

        // Populate doc with sensor data (similar to handleGetAllSensorData)
        JsonArray plants = doc["plants"].to<JsonArray>();
        for (int i = 0; i < 4; i++) {
            JsonObject plant = plants.add<JsonObject>();
            plant["moisture"] = sensorData.moisture[i];
        }

        doc["temperature"] = sensorData.temperature;
        doc["pressure"] = sensorData.pressure;
        doc["waterLevel"] = sensorData.waterLevel;

        // Add relay states
        JsonArray relays = doc["relays"].to<JsonArray>();
        for (int i = 0; i < RELAY_COUNT; i++) {
            JsonObject relay = relays.add<JsonObject>();
            relay["active"] = relayManager.isRelayActive(i);
            if (relayManager.isRelayActive(i)) {
                relay["activationTime"] = configManager.getSensorConfig(i).activationPeriod;
            }
        }

        String output;
        serializeJson(doc, output);
        // Send update to all connected SSE clients
        events->send(output.c_str(), "update", millis());
    }

    // Call this method whenever sensor data or relay states change
    void notifyClients() {
        sendUpdate();
    }

};

#endif // ESP32_WEB_SERVER_H