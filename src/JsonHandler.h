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

        int systemSize = configManager.getHwConfig().systemSize.value();
        for (size_t i = 0; i < systemSize; ++i) {
            const auto& config = configManager.getSensorConfig(i);
            
            JsonObject plant = plants.add<JsonObject>();
            plant["index"] = i;
            plant["moisture"] = sensorData.moisture[i];
            plant["enabled"] = config.sensorEnabled.value();

            JsonObject relay = relays.add<JsonObject>();
            relay["index"] = i;
            relay["active"] = relayManager.getRelayState(i);
            relay["enabled"] = config.relayEnabled.value();
            if (relay["active"]) {
                relay["activationTime"] = config.activationPeriod.value();
            }
        }

        return doc;
    }

    static JsonDocument createSetupJson(const ConfigManager& configManager) {
        JsonDocument doc;

        const ConfigTypes::HardwareConfig& hwConfig = configManager.getHwConfig();
        doc["systemSize"] = hwConfig.systemSize.value();
        doc["sdaPin"] = hwConfig.sdaPin.value();
        doc["sclPin"] = hwConfig.sclPin.value();
        doc["floatSwitchPin"] = hwConfig.floatSwitchPin.value();

        // Handle vector types
        // Updated: Use to<JsonArray>() instead of createNestedArray()
        JsonArray sensorPinsArray = doc["sensorPins"].to<JsonArray>();
        for (const auto& pin : hwConfig.moistureSensorPins) {
            sensorPinsArray.add(pin);
        }

        // Updated: Use to<JsonArray>() instead of createNestedArray()
        JsonArray relayPinsArray = doc["relayPins"].to<JsonArray>();
        for (const auto& pin : hwConfig.relayPins) {
            relayPinsArray.add(pin);
        }
        return doc;
    }

    static JsonDocument createConfigJson(const ConfigManager& configManager) {
        JsonDocument doc;

        const ConfigTypes::HardwareConfig& hwConfig = configManager.getHwConfig();
        const ConfigTypes::SoftwareConfig& swConfig = configManager.getSwConfig();
        doc["temperatureOffset"] = swConfig.tempOffset.value();
        doc["telemetryInterval"] = swConfig.telemetryInterval.value();
        doc["sensorUpdateInterval"] = swConfig.sensorUpdateInterval.value();
        doc["lcdUpdateInterval"] = swConfig.lcdUpdateInterval.value();
        doc["sensorPublishInterval"] = swConfig.sensorPublishInterval.value();


        JsonArray sensorConfigs = doc["sensorConfigs"].to<JsonArray>();
        for (size_t i = 0; i < hwConfig.systemSize.value(); i++) {
            const auto& config = configManager.getSensorConfig(i);
            JsonObject sensorObj = sensorConfigs.add<JsonObject>();
            sensorObj["threshold"] = config.threshold.value();
            sensorObj["activationPeriod"] = config.activationPeriod.value();
            sensorObj["wateringInterval"] = config.wateringInterval.value();
            sensorObj["sensorEnabled"] = config.sensorEnabled.value();
            sensorObj["relayEnabled"] = config.relayEnabled.value();
        }
        return doc;
    }

    static bool updateSetup(ConfigManager& configManager, const JsonDocument& doc) {
        if (doc.containsKey("sdaPin") || doc.containsKey("sclPin") || doc.containsKey("floatSwitchPin") || doc.containsKey("systemSize")) { 
            ConfigTypes::HardwareConfig hwConfig = configManager.getHwConfig();
            updateHardwareConfig(hwConfig, doc);
            configManager.setHardwareConfig(hwConfig);
        }
        return true;
    }

    static bool updateConfig(ConfigManager& configManager, const JsonDocument& doc) {
        if (doc.containsKey("temperatureOffset") || doc.containsKey("telemetryInterval") ||
            doc.containsKey("sensorUpdateInterval") || doc.containsKey("lcdUpdateInterval") ||
            doc.containsKey("sensorPublishInterval")) {
                ConfigTypes::SoftwareConfig swConfig = configManager.getSwConfig();
                updateSoftwareConfig(swConfig, doc);
                configManager.setSoftwareConfig(swConfig);
        }

        if (doc.containsKey("sensorConfigs")) {
            JsonArrayConst sensorConfigs = doc["sensorConfigs"].as<JsonArrayConst>();
            for (size_t i = 0; i < std::min(sensorConfigs.size(), static_cast<size_t>(configManager.getHwConfig().systemSize.value())); i++) {
                ConfigTypes::SensorConfig newConfig = configManager.getSensorConfig(i);
                if (sensorConfigs[i].is<JsonObjectConst>()) {
                    ConfigTypes::SensorConfig newConfig = configManager.getSensorConfig(i);
                    JsonObjectConst sensorConfig = sensorConfigs[i].as<JsonObjectConst>();
                    updateSensorConfig(newConfig, sensorConfig);
                    configManager.setSensorConfig(newConfig, i);
                } 
                else {
                    return false;
                }
            }
        }

        return true;
    }

private:
    static void updateSoftwareConfig(ConfigTypes::SoftwareConfig& config, const JsonDocument& doc) {
        if (doc.containsKey("temperatureOffset")) config.tempOffset = doc["temperatureOffset"].as<float>();
        if (doc.containsKey("telemetryInterval")) config.telemetryInterval = doc["telemetryInterval"].as<uint32_t>();
        if (doc.containsKey("sensorUpdateInterval")) config.sensorUpdateInterval = doc["sensorUpdateInterval"].as<uint32_t>();
        if (doc.containsKey("lcdUpdateInterval")) config.lcdUpdateInterval = doc["lcdUpdateInterval"].as<uint32_t>();
        if (doc.containsKey("sensorPublishInterval")) config.sensorPublishInterval = doc["sensorPublishInterval"].as<uint32_t>();
    }

    static void updateSensorConfig(ConfigTypes::SensorConfig& config, const JsonDocument& jsonConfig) {
        if (jsonConfig.containsKey("threshold")) config.threshold = jsonConfig["threshold"].as<float>();
        if (jsonConfig.containsKey("activationPeriod")) config.activationPeriod = jsonConfig["activationPeriod"].as<uint32_t>();
        if (jsonConfig.containsKey("wateringInterval")) config.wateringInterval = jsonConfig["wateringInterval"].as<uint32_t>();
        if (jsonConfig.containsKey("sensorEnabled")) config.sensorEnabled = jsonConfig["sensorEnabled"].as<bool>();
        if (jsonConfig.containsKey("relayEnabled")) config.relayEnabled = jsonConfig["relayEnabled"].as<bool>();
    }

    static void updateHardwareConfig(ConfigTypes::HardwareConfig& config, const JsonDocument& doc) {
        if (doc.containsKey("systemSize")) config.systemSize = doc["systemSize"].as<int>();
        if (doc.containsKey("sdaPin")) config.sdaPin = doc["sdaPin"].as<int>();
        if (doc.containsKey("sclPin")) config.sclPin = doc["sclPin"].as<int>();
        if (doc.containsKey("floatSwitchPin")) config.floatSwitchPin = doc["floatSwitchPin"].as<int>();
        if (doc.containsKey("relayPins") && doc["relayPins"].is<JsonArrayConst>()) {
            config.relayPins.clear();
            for (JsonVariantConst v : doc["relayPins"].as<JsonArrayConst>()) {
                config.relayPins.push_back(v.as<int>());
            }
        }
        if (doc.containsKey("sensorPins") && doc["sensorPins"].is<JsonArrayConst>()) {
            config.moistureSensorPins.clear();
            for (JsonVariantConst v : doc["sensorPins"].as<JsonArrayConst>()) {
                config.moistureSensorPins.push_back(v.as<int>());
            }
        }
    }
};

#endif // JSON_HANDLER_H