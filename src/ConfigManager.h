#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Preferences.h>
#include <map>
#include <string>
#include <functional>
#include <array>
#include <vector>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <variant>
#include "Globals.h"
#include "ESPLogger.h"
#include "ConfigTypes.h"
#include "PreferencesHandler.h"


class ConfigManager {
public:
    ConfigManager(PreferencesHandler& preferencesHandler) : logger(Logger::instance()),
                                                            preferences(), 
                                                            prefsHandler(preferencesHandler){}

    // @initializeDefaultsValues is called to fill the pref. library with defaults, only for the first run
    // @initializeConfigurations is called to fill the runtime cache so that other classes can access it.
    bool begin(const char* name = "cf") {
        bool result = preferences.begin(name, false);
        if (result) {
            size_t storedSize = getValue<int>(ConfigKey::SYSTEM_SIZE, 0);
            size_t currentSize = std::get<int>(configMap.at(ConfigKey::SYSTEM_SIZE).defaultValue);

            if (!preferences.isKey("initialized")) {
                logger.log("ConfigManager", LogLevel::INFO, "First run, initializing default values");
                initializeDefaultValues(currentSize);
                preferences.putBool("initialized", true);
            } else if (currentSize > storedSize) {
                logger.log("ConfigManager", LogLevel::INFO, "System size increased, initializing new sensors");
                initializeNewSensors(storedSize, currentSize);
            }

            prefsHandler.saveToPreferences(ConfigKey::SYSTEM_SIZE, currentSize, 0);
        }

        initializeConfigurations();
        return result;
    }
    
    void initializeConfigurations() {

        size_t oldSystemSize = sensorConf.size();
        size_t newSystemSize = getValue<int>(ConfigKey::SYSTEM_SIZE, 0);

        if (newSystemSize < oldSystemSize) {
            cleanupRemovedSensors(newSystemSize, oldSystemSize);
        }

        createHardwareConfig(newSystemSize);
        createSoftwareConfig();

        sensorConf.resize(newSystemSize);
        
        for (size_t i = 0; i < sensorConf.size(); i++) {
            createSensorConfig(i);
        }
    }

    // return a reference to the specified config structure
    const ConfigTypes::HardwareConfig& getHwConfig() const {
        return hwConf;
    }
    const ConfigTypes::SoftwareConfig& getSwConfig() const {
        return swConf;
    }
    const ConfigTypes::SensorConfig& getSensorConfig(size_t index) const{
        return sensorConf[index];
    }

    // Grouped setter for HardwareConfig with partial update support
    bool setHardwareConfig(const ConfigTypes::HardwareConfig& newConfig) {
        std::unique_lock lock(mutex);
        bool changed = false;
        
        if (newConfig.sdaPin) changed |= setAndSave<int>(ConfigKey::SDA_PIN, *newConfig.sdaPin, hwConf.sdaPin);
        if (newConfig.sclPin) changed |= setAndSave(ConfigKey::SCL_PIN, *newConfig.sclPin, hwConf.sclPin);
        if (newConfig.floatSwitchPin) changed |= setAndSave(ConfigKey::FLOAT_SWITCH_PIN, *newConfig.floatSwitchPin, hwConf.floatSwitchPin);
        if (!newConfig.moistureSensorPins.empty()) changed |= setAndSave(ConfigKey::SENSOR_PIN, newConfig.moistureSensorPins, hwConf.moistureSensorPins);
        if (!newConfig.relayPins.empty()) changed |= setAndSave(ConfigKey::RELAY_PIN, newConfig.relayPins, hwConf.relayPins);
        if (newConfig.systemSize) changed |= setAndSave(ConfigKey::SYSTEM_SIZE, *newConfig.systemSize, hwConf.systemSize);

        return changed;
    }

    // Grouped setter for SoftwareConfig with partial update support
    bool setSoftwareConfig(const ConfigTypes::SoftwareConfig& newConfig) {
        std::unique_lock lock(mutex);
        bool changed = false;
        
        if (newConfig.tempOffset) changed |= setAndSave(ConfigKey::TEMP_OFFSET, *newConfig.tempOffset, swConf.tempOffset);
        if (newConfig.telemetryInterval) changed |= setAndSave(ConfigKey::TELEMETRY_INTERVAL, *newConfig.telemetryInterval, swConf.telemetryInterval);
        if (newConfig.sensorUpdateInterval) changed |= setAndSave(ConfigKey::SENSOR_UPDATE_INTERVAL, *newConfig.sensorUpdateInterval, swConf.sensorUpdateInterval);
        if (newConfig.lcdUpdateInterval) changed |= setAndSave(ConfigKey::LCD_UPDATE_INTERVAL, *newConfig.lcdUpdateInterval, swConf.lcdUpdateInterval);
        if (newConfig.sensorPublishInterval) changed |= setAndSave(ConfigKey::SENSOR_PUBLISH_INTERVAL, *newConfig.sensorPublishInterval, swConf.sensorPublishInterval);

        return changed;
    }

    // Grouped setter for SensorConfig with partial update support
    bool setSensorConfig(const ConfigTypes::SensorConfig& newConfig, size_t sensorIndex) {
        std::unique_lock lock(mutex);
        if (sensorIndex >= sensorConf.size()) {
            logger.log("ConfigManager", LogLevel::ERROR, "Invalid sensor index");
            return false;
        }

        bool changed = false;
        ConfigTypes::SensorConfig& currentConfig = sensorConf[sensorIndex];
        
        if (newConfig.threshold) changed |= setAndSave(ConfigKey::SENSOR_THRESHOLD, *newConfig.threshold, currentConfig.threshold, sensorIndex);
        if (newConfig.activationPeriod) changed |= setAndSave(ConfigKey::SENSOR_ACTIVATION_PERIOD, *newConfig.activationPeriod, currentConfig.activationPeriod, sensorIndex);
        if (newConfig.wateringInterval) changed |= setAndSave(ConfigKey::SENSOR_WATERING_INTERVAL, *newConfig.wateringInterval, currentConfig.wateringInterval, sensorIndex);
        if (newConfig.sensorEnabled) changed |= setAndSave(ConfigKey::SENSOR_ENABLED, *newConfig.sensorEnabled, currentConfig.sensorEnabled, sensorIndex);
        if (newConfig.relayEnabled) changed |= setAndSave(ConfigKey::RELAY_ENABLED, *newConfig.relayEnabled, currentConfig.relayEnabled, sensorIndex);

        return changed;
    }
    void resetToDefault() {}

private:
    Preferences preferences;
    PreferencesHandler& prefsHandler;
    Logger& logger;
    mutable std::shared_mutex mutex;
    
    ConfigTypes::HardwareConfig hwConf;
    ConfigTypes::SoftwareConfig swConf;
    std::vector<ConfigTypes::SensorConfig> sensorConf;

    static inline const std::map<ConfigKey, ConfigInfo>& configMap = ::configMap;

    void createSensorConfig(size_t index) {
        ConfigTypes::SensorConfig& conf = sensorConf[index];
        conf.threshold = getValue<float>(ConfigKey::SENSOR_THRESHOLD, index);
        conf.activationPeriod = getValue<uint32_t>(ConfigKey::SENSOR_ACTIVATION_PERIOD, index);
        conf.wateringInterval = getValue<uint32_t>(ConfigKey::SENSOR_WATERING_INTERVAL, index);
        conf.sensorEnabled = getValue<bool>(ConfigKey::SENSOR_ENABLED, index);
        conf.relayEnabled = getValue<bool>(ConfigKey::RELAY_ENABLED, index);
    }

    void createSoftwareConfig() {
        swConf.tempOffset = getValue<float>(ConfigKey::TEMP_OFFSET);
        swConf.telemetryInterval = getValue<uint32_t>(ConfigKey::TELEMETRY_INTERVAL);
        swConf.sensorUpdateInterval = getValue<uint32_t>(ConfigKey::SENSOR_UPDATE_INTERVAL);
        swConf.lcdUpdateInterval = getValue<uint32_t>(ConfigKey::LCD_UPDATE_INTERVAL);
        swConf.sensorPublishInterval = getValue<uint32_t>(ConfigKey::SENSOR_PUBLISH_INTERVAL);
    }
    
    void createHardwareConfig(size_t systemSize) { 
        hwConf.sdaPin = getValue<int>(ConfigKey::SDA_PIN);
        hwConf.sclPin = getValue<int>(ConfigKey::SCL_PIN);
        hwConf.floatSwitchPin = getValue<int>(ConfigKey::FLOAT_SWITCH_PIN);
        hwConf.moistureSensorPins = adjustVector(ConfigKey::SENSOR_PIN, systemSize);
        hwConf.relayPins = adjustVector(ConfigKey::RELAY_PIN, systemSize);
    }

    template<typename T>
    bool validateValue(ConfigKey key, const T& value) const {
        const auto& info = configMap.at(key);
        if (info.minValue && info.maxValue) {
            return std::visit([&value](const auto& min, const auto& max) -> bool {
                using MinMaxType = std::decay_t<decltype(min)>;
                if constexpr (std::is_convertible_v<T, MinMaxType>) {
                    return static_cast<MinMaxType>(value) >= min && static_cast<MinMaxType>(value) <= max;
                }
                return false;
            }, *info.minValue, *info.maxValue);
        }
        return true;
    }

    template<typename T>
    T getValue(ConfigKey key, std::optional<size_t> index = std::nullopt) const {
        std::shared_lock lock(mutex);
        const auto& info = configMap.at(key);
        
        if (info.confType != "sensorConf") {
            index = 0;  // Use index 0 for non-sensor configs
        } else if (!index.has_value()) {
            throw std::runtime_error("Index must be provided for sensor-specific configurations");
        }
        
        return std::visit([this, key, index](const auto& defaultVal) -> T {
            using DefaultType = std::decay_t<decltype(defaultVal)>;
            if constexpr (std::is_convertible_v<DefaultType, T>) {
                return prefsHandler.loadFromPreferences<T>(key, static_cast<T>(defaultVal), index.value());
            } else {
                throw std::runtime_error("Type mismatch in getValue");
            }
        }, info.defaultValue);
    }

    template<typename T>
    bool setAndSave(ConfigKey key, const T& newValue, T& currentValue, size_t sensorIndex = 0) {
        if (!validateValue(key, newValue)) {
            logger.log("ConfigManager", LogLevel::ERROR, "Invalid value for config");
            return false;
        }

        if (currentValue != newValue) {
            currentValue = newValue;  // Update runtime cache
            prefsHandler.saveToPreferences(key, newValue, sensorIndex);  // Update persistent storage
            return true;
        }
        return false;
    }

    // Specialized version for std::optional types
    template<typename T>
    bool setAndSave(ConfigKey key, const T& newValue, std::optional<T>& currentValue, size_t sensorIndex = 0) {
        if (!validateValue(key, newValue)) {
            logger.log("ConfigManager", LogLevel::ERROR, "Invalid value for config");
            return false;
        }

        if (!currentValue || *currentValue != newValue) {
            currentValue = newValue;  // Update runtime cache
            prefsHandler.saveToPreferences(key, newValue, sensorIndex);  // Update persistent storage
            return true;
        }
        return false;
    }

    void initializeDefaultValues(size_t systemSize) {
        logger.log("ConfigManager", LogLevel::INFO, "Initializing default values");
        for (const auto& [key, info] : configMap) {
            std::visit([this, key, &info](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float> || 
                            std::is_same_v<T, bool> || std::is_same_v<T, uint32_t> ||
                            std::is_same_v<T, std::vector<int>>) {
                    prefsHandler.saveToPreferences(key, arg, 0);  // Use index 0 for global configs
                }
            }, info.defaultValue);
        }

        // For sensor-specific configurations, we need to initialize for each sensor
        for (size_t i = 0; i < systemSize; ++i) {
            for (const auto& [key, info] : configMap) {
                if (info.confType == "sensorConf") {
                    std::visit([this, key, i](auto&& arg) {
                        using T = std::decay_t<decltype(arg)>;
                        prefsHandler.saveToPreferences(key, arg, i);
                    }, info.defaultValue);
                }
            }
        }

        logger.log("ConfigManager", LogLevel::INFO, "Default values initialized");
    }

    void initializeNewSensors(size_t oldSize, size_t newSize) {
        for (size_t i = oldSize; i < newSize; i++) {
            for (const auto& [key, info] : configMap) {
                if (info.confType == "sensorConf") {
                    std::visit([this, key, i](auto&& arg) {
                        using T = std::decay_t<decltype(arg)>;
                        prefsHandler.saveToPreferences(key, arg, i);
                    }, info.defaultValue);
                }
            }
        }
    }

    std::vector<int> adjustVector(ConfigKey key, size_t newSize) {
        std::vector<int> vec = getValue<std::vector<int>>(key);
        
        if (vec.size() != newSize) {
            const auto& info = configMap.at(key);
            const auto& defaultVec = std::get<std::vector<int>>(info.defaultValue);
            
            if (vec.size() < newSize) {
                // Expand
                while (vec.size() < newSize) {
                    vec.push_back(defaultVec[vec.size() % defaultVec.size()]);
                }
            } else {
                // Shrink
                vec.resize(newSize);
            }
            
            // Save the adjusted vector back to preferences
            prefsHandler.saveToPreferences(key, vec, 0);
        }
        
        return vec;
    }

    void cleanupRemovedSensors(size_t newSize, size_t oldSize) {
        for (size_t i = newSize; i < oldSize; i++) {
            for (const auto& [key, info] : configMap) {
                if (info.confType == "sensorConf") {
                    prefsHandler.removeFromPreferences(key, i);
                }
            }
        }
    }
};

#endif // CONFIG_MANAGER_H