#ifndef ESP32_WEB_SERVER_H
#define ESP32_WEB_SERVER_H

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ESPLogger.h"
#include "SensorManager.h"
#include "ConfigManager.h"

extern SensorManager sensorManager;
extern ConfigManager configManager;

class ESP32WebServer {
private:
    AsyncWebServer server;
    int serverPort;
    ESPLogger& logger;

    void setupRoutes() {
        // Serve static files
        server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

        // API endpoints
        server.on("/api/logs", HTTP_GET, std::bind(&ESP32WebServer::handleGetLogs, this, std::placeholders::_1));
        server.on("/api/config", HTTP_GET, std::bind(&ESP32WebServer::handleGetConfig, this, std::placeholders::_1));
        server.on("/api/config", HTTP_POST, std::bind(&ESP32WebServer::handlePostConfig, this, std::placeholders::_1));
        server.on("/api/sensorData", HTTP_GET, std::bind(&ESP32WebServer::handleGetSensorData, this, std::placeholders::_1));
        //server.on("/api/relay", HTTP_POST, std::bind(&ESP32WebServer::handlePostRelay, this, std::placeholders::_1));
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

        JsonArray sensorConfigs = doc.createNestedArray("sensorConfigs");
        for (size_t i = 0; i < ConfigManager::RELAY_COUNT; i++) {
            JsonObject sensorConfig = sensorConfigs.createNestedObject();
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

            // Parse sensor configs
            if (doc.containsKey("sensorConfigs")) {
                JsonArray sensorConfigs = doc["sensorConfigs"];
                for (size_t i = 0; i < std::min(static_cast<size_t>(sensorConfigs.size()), ConfigManager::RELAY_COUNT); i++) {
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

        // Assuming SensorManager has a method to get all sensor data
        const auto& sensorData = sensorManager.getSensorData();
        // Add sensor data to the JSON document

        serializeJson(doc, *response);
        request->send(response);
    }

    // void handlePostRelay(AsyncWebServerRequest *request) {
    //     if (request->hasParam("relay", true) && request->hasParam("state", true)) {
    //         int relay = request->getParam("relay", true)->value().toInt();
    //         bool state = request->getParam("state", true)->value() == "true";
            
    //         if (sensorManager.setRelayState(relay, state)) {
    //             logger.log(LogLevel::INFO, "Relay %d set to %s", relay, state ? "ON" : "OFF");
    //             request->send(200, "text/plain", "Relay state updated");
    //         } else {
    //             logger.log(LogLevel::ERROR, "Failed to set relay %d state", relay);
    //             request->send(500, "text/plain", "Failed to set relay state");
    //         }
    //     } else {
    //         request->send(400, "text/plain", "Missing relay or state parameter");
    //     }
    // }

public:
    ESP32WebServer(int port, ESPLogger& logger)
        : server(port), logger(logger), serverPort(port) {
        setupRoutes();
    }

    void begin() {
        server.begin();
        logger.log(LogLevel::INFO, "Async HTTP server started on port %d", serverPort);
    }
};

#endif // ESP32_WEB_SERVER_H