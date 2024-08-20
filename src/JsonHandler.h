#ifndef JSON_HANDLER_H
#define JSON_HANDLER_H

#include <ArduinoJson.h>
#include "ConfigManager.h"
#include "SensorManager.h"
#include "RelayManager.h"

class JsonHandler {
public:
    static JsonDocument createSensorDataJson(SensorManager& sensorManager, 
                                             RelayManager& relayManager, 
                                             ConfigManager& configManager) {
        JsonDocument doc;
        const  SensorData& sensorData = sensorManager.getSensorData();

        doc["temperature"] = sensorData.temperature;
        doc["pressure"] = sensorData.pressure;
        doc["waterLevel"] = sensorData.waterLevel;

        JsonArray plants = doc["plants"].to<JsonArray>();
        JsonArray relays = doc["relays"].to<JsonArray>();

        for (const auto& [pin, moisture] : sensorData.moisture) {
            auto config = configManager.getSensorConfig(pin);
            if (config) {
                plants.add(createPlantObject(pin, moisture, config->sensorEnabled));
                relays.add(createRelayObject(pin, sensorData.relayState.at(config->relayPin), 
                                             config->relayEnabled, config->activationPeriod));
            }
        }

        return doc;
    }

    static JsonDocument createConfigJson(const ConfigManager& configManager) {
        JsonDocument doc;

        const ConfigManager::SoftwareConfig& swConfig = configManager.getSoftwareConfig();
        doc["temperatureOffset"] = swConfig.tempOffset;
        doc["telemetryInterval"] = swConfig.telemetryInterval;
        doc["sensorUpdateInterval"] = swConfig.sensorUpdateInterval;
        doc["lcdUpdateInterval"] = swConfig.lcdUpdateInterval;
        doc["sensorPublishInterval"] = swConfig.sensorPublishInterval;

        JsonArray sensorConfigs = doc["sensorConfigs"].to<JsonArray>();
        for (size_t i = 0; i < ConfigConstants::RELAY_COUNT; i++) {
            if (auto config = configManager.getSensorConfig(i)) {
                sensorConfigs.add(createSensorConfigObject(*config));
            }
        }

        return doc;
    }

    static bool updateConfig(ConfigManager& configManager, const JsonDocument& doc) {
        if (doc.containsKey("temperatureOffset")) {
            ConfigManager::SoftwareConfig swConfig = configManager.getSoftwareConfig();
            updateSoftwareConfig(swConfig, doc);
            configManager.setSoftwareConfig(swConfig);
        }
        
        if (doc.containsKey("sensorConfigs")) {
            JsonArrayConst sensorConfigs = doc["sensorConfigs"].as<JsonArrayConst>();
            for (size_t i = 0; i < std::min(sensorConfigs.size(), static_cast<size_t>(ConfigConstants::RELAY_COUNT)); i++) {
                if (auto currentConfig = configManager.getSensorConfig(i)) {
                    ConfigManager::SensorConfig newConfig = *currentConfig;
                    if (sensorConfigs[i].is<JsonObjectConst>()) {
                        JsonObjectConst sensorConfig = sensorConfigs[i].as<JsonObjectConst>();
                        if (updateSensorConfig(newConfig, sensorConfig)) {
                            configManager.setSensorConfig(i, newConfig);
                        } else {
                            return false;
                        }
                    } else {
                        return false;
                    }
                }
            }
        }

        return true;
    }

private:
    static JsonObject createPlantObject(int index, float moisture, bool enabled) {
        JsonObject plant;
        plant["index"] = index;
        plant["moisture"] = moisture;
        plant["enabled"] = enabled;
        return plant;
    }

    static JsonObject createRelayObject(int index, bool active, bool enabled, uint32_t activationTime) {
        JsonObject relay;
        relay["index"] = index;
        relay["active"] = active;
        relay["enabled"] = enabled;
        if (active) {
            relay["activationTime"] = activationTime;
        }
        return relay;
    }

    static JsonObject createSensorConfigObject(const ConfigManager::SensorConfig& config) {
        JsonObject sensorConfig;
        sensorConfig["threshold"] = config.threshold;
        sensorConfig["activationPeriod"] = config.activationPeriod;
        sensorConfig["wateringInterval"] = config.wateringInterval;
        sensorConfig["sensorEnabled"] = config.sensorEnabled;
        sensorConfig["relayEnabled"] = config.relayEnabled;
        sensorConfig["sensorPin"] = config.sensorPin;
        sensorConfig["relayPin"] = config.relayPin;
        return sensorConfig;
    }

    static void updateSoftwareConfig(ConfigManager::SoftwareConfig& config, const JsonDocument& doc) {
        if (doc.containsKey("temperatureOffset")) config.tempOffset = doc["temperatureOffset"].as<float>();
        if (doc.containsKey("telemetryInterval")) config.telemetryInterval = doc["telemetryInterval"].as<uint32_t>();
        if (doc.containsKey("sensorUpdateInterval")) config.sensorUpdateInterval = doc["sensorUpdateInterval"].as<uint32_t>();
        if (doc.containsKey("lcdUpdateInterval")) config.lcdUpdateInterval = doc["lcdUpdateInterval"].as<uint32_t>();
        if (doc.containsKey("sensorPublishInterval")) config.sensorPublishInterval = doc["sensorPublishInterval"].as<uint32_t>();
    }

    static bool updateSensorConfig(ConfigManager::SensorConfig& config, const JsonDocument& jsonConfig) {
        if (jsonConfig.containsKey("threshold")) config.threshold = jsonConfig["threshold"].as<float>();
        if (jsonConfig.containsKey("activationPeriod")) config.activationPeriod = jsonConfig["activationPeriod"].as<uint32_t>();
        if (jsonConfig.containsKey("wateringInterval")) config.wateringInterval = jsonConfig["wateringInterval"].as<uint32_t>();
        if (jsonConfig.containsKey("sensorEnabled")) config.sensorEnabled = jsonConfig["sensorEnabled"].as<bool>();
        if (jsonConfig.containsKey("relayEnabled")) config.relayEnabled = jsonConfig["relayEnabled"].as<bool>();
        if (jsonConfig.containsKey("sensorPin")) config.sensorPin = jsonConfig["sensorPin"].as<int>();
        if (jsonConfig.containsKey("relayPin")) config.relayPin = jsonConfig["relayPin"].as<int>();
        return true;
    }
};

#endif // JSON_HANDLER_H