#ifndef JSON_HANDLER_H
#define JSON_HANDLER_H

#include <ArduinoJson.h>
#include "ConfigManager.h"
#include "SensorManager.h"
#include "RelayManager.h"

class JsonHandler {
public:
    JsonDocument createSensorDataJson(const SensorManager& sensorManager, 
                                const RelayManager& relayManager, 
                                const ConfigManager& configManager) {
    JsonDocument doc;
    const SensorData& sensorData = sensorManager.getSensorData();

    doc["temperature"] = sensorData.temperature;
    doc["pressure"] = sensorData.pressure;
    doc["waterLevel"] = sensorData.waterLevel;

    JsonArray plants = doc["plants"].to<JsonArray>();
    JsonArray relays = doc["relays"].to<JsonArray>();

    for (size_t i = 0; i < ConfigConstants::RELAY_COUNT; ++i) {
        const auto& config = configManager.getCachedSensorConfig(i);
        
        JsonObject plant = plants.add<JsonObject>();
        plant["index"] = i;
        plant["moisture"] = sensorData.moisture[i];
        plant["enabled"] = config.sensorEnabled;

        JsonObject relay = relays.add<JsonObject>();
        relay["index"] = i;
        relay["active"] = relayManager.getRelayState(i);
        relay["enabled"] = config.relayEnabled;
        if (relay["active"]) {
            relay["activationTime"] = config.activationPeriod;
        }
    }

    return doc;
}
    static JsonDocument createConfigJson(const ConfigManager& configManager) {
        JsonDocument doc;

        const ConfigManager::SoftwareConfig& swConfig = configManager.getCachedSoftwareConfig();
        doc["temperatureOffset"] = swConfig.tempOffset;
        doc["telemetryInterval"] = swConfig.telemetryInterval;
        doc["sensorUpdateInterval"] = swConfig.sensorUpdateInterval;
        doc["lcdUpdateInterval"] = swConfig.lcdUpdateInterval;
        doc["sensorPublishInterval"] = swConfig.sensorPublishInterval;

        const ConfigManager::HardwareConfig& hwConfig = configManager.getCachedHardwareConfig();
        doc["sdaPin"] = hwConfig.sdaPin;
        doc["sclPin"] = hwConfig.sclPin;
        doc["floatSwitchPin"] = hwConfig.floatSwitchPin;

        JsonArray sensorConfigs = doc["sensorConfigs"].to<JsonArray>();
        for (size_t i = 0; i < ConfigConstants::RELAY_COUNT; i++) {
            const auto& config = configManager.getCachedSensorConfig(i);
            JsonObject sensorObj = sensorConfigs.add<JsonObject>();
            sensorObj["threshold"] = config.threshold;
            sensorObj["activationPeriod"] = config.activationPeriod;
            sensorObj["wateringInterval"] = config.wateringInterval;
            sensorObj["sensorEnabled"] = config.sensorEnabled;
            sensorObj["relayEnabled"] = config.relayEnabled;
            sensorObj["sensorPin"] = config.sensorPin;
            sensorObj["relayPin"] = config.relayPin;
        }
        return doc;
    }


    static bool updateConfig(ConfigManager& configManager, const JsonDocument& doc) {
        if (doc.containsKey("temperatureOffset") || doc.containsKey("telemetryInterval") ||
            doc.containsKey("sensorUpdateInterval") || doc.containsKey("lcdUpdateInterval") ||
            doc.containsKey("sensorPublishInterval")) {
            ConfigManager::SoftwareConfig swConfig = configManager.getCachedSoftwareConfig();
            updateSoftwareConfig(swConfig, doc);
            configManager.setSoftwareConfig(swConfig);
        }
        
        if (doc.containsKey("sdaPin") || doc.containsKey("sclPin") || doc.containsKey("floatSwitchPin")) {
            ConfigManager::HardwareConfig hwConfig = configManager.getCachedHardwareConfig();
            updateHardwareConfig(hwConfig, doc);
            configManager.setHardwareConfig(hwConfig);
        }

        if (doc.containsKey("sensorConfigs")) {
            JsonArrayConst sensorConfigs = doc["sensorConfigs"].as<JsonArrayConst>();
            for (size_t i = 0; i < std::min(sensorConfigs.size(), static_cast<size_t>(ConfigConstants::RELAY_COUNT)); i++) {
                ConfigManager::SensorConfig newConfig = configManager.getCachedSensorConfig(i);
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

        return true;
    }

private:
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

    static void updateHardwareConfig(ConfigManager::HardwareConfig& config, const JsonDocument& doc) {
        if (doc.containsKey("sdaPin")) config.sdaPin = doc["sdaPin"].as<int>();
        if (doc.containsKey("sclPin")) config.sclPin = doc["sclPin"].as<int>();
        if (doc.containsKey("floatSwitchPin")) config.floatSwitchPin = doc["floatSwitchPin"].as<int>();
    }
};

#endif // JSON_HANDLER_H