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

class ConfigManager {
public:
    struct SensorConfig {
        float threshold;
        uint32_t activationPeriod;
        uint32_t wateringInterval;
        bool sensorEnabled;
        bool relayEnabled;
        int sensorPin;
        int relayPin;
    };

    struct HardwareConfig {
        int sdaPin;
        int sclPin;
        int floatSwitchPin;
        std::array<int, ConfigConstants::RELAY_COUNT> moistureSensorPins;
        std::array<int, ConfigConstants::RELAY_COUNT> relayPins;
    };

    struct SoftwareConfig {
        float tempOffset;
        uint32_t telemetryInterval;
        uint32_t sensorUpdateInterval;
        uint32_t lcdUpdateInterval;
        uint32_t sensorPublishInterval;
    };

    enum class ConfigKey {
        SENSOR_THRESHOLD,
        SENSOR_ACTIVATION_PERIOD,
        SENSOR_WATERING_INTERVAL,
        SENSOR_ENABLED,
        RELAY_ENABLED,
        SENSOR_PIN,
        RELAY_PIN,
        SDA_PIN,
        SCL_PIN,
        FLOAT_SWITCH_PIN,
        TEMP_OFFSET,
        TELEMETRY_INTERVAL,
        SENSOR_UPDATE_INTERVAL,
        LCD_UPDATE_INTERVAL,
        SENSOR_PUBLISH_INTERVAL
    };

    using ConfigValue = std::variant<bool, int, float, uint32_t>;
    using ObserverCallback = std::function<void(ConfigKey)>;
    ConfigManager() : logger(Logger::instance()),
                      preferences() {}

    bool begin(const char* name = "irrigation-config") {
        std::unique_lock<std::shared_mutex> lock(mutex);
        return preferences.begin(name, false);
    }

    void end() {
        std::unique_lock<std::shared_mutex> lock(mutex);
        preferences.end();
    }

    void setValue(ConfigKey key, ConfigValue value, size_t index = 0) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        std::string prefKey = getPreferenceKey(key, index);
        if (setValueInternal(prefKey, value)) {
            notifyObservers(key);
        } else {
            logger.log("ConfigManager", "ERROR", "Failed to save value for key: " + prefKey);
        }
    }

    ConfigValue getValue(ConfigKey key, size_t index = 0) const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        std::string prefKey = getPreferenceKey(key, index);
        auto value = getValueInternal(prefKey);
        if (!value) {
            logger.log("ConfigManager", "WARNING", "Value not found for key: " + prefKey + ". Using default.");
            return defaultValues.at(key);
        }
        return *value;
    }

    void setSensorConfig(size_t index, const SensorConfig& config) {
        if (index >= ConfigConstants::RELAY_COUNT) {
            logger.log("ConfigManager", "ERROR", "Invalid sensor index: " + std::to_string(index));
            return;
        }

        std::unique_lock<std::shared_mutex> lock(mutex);
        setValue(ConfigKey::SENSOR_THRESHOLD, config.threshold, index);
        setValue(ConfigKey::SENSOR_ACTIVATION_PERIOD, config.activationPeriod, index);
        setValue(ConfigKey::SENSOR_WATERING_INTERVAL, config.wateringInterval, index);
        setValue(ConfigKey::SENSOR_ENABLED, config.sensorEnabled, index);
        setValue(ConfigKey::RELAY_ENABLED, config.relayEnabled, index);
        setValue(ConfigKey::SENSOR_PIN, config.sensorPin, index);
        setValue(ConfigKey::RELAY_PIN, config.relayPin, index);

        notifyObservers(ConfigKey::SENSOR_THRESHOLD);
    }

    void setSoftwareConfig(const SoftwareConfig& config) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        setValue(ConfigKey::TEMP_OFFSET, config.tempOffset);
        setValue(ConfigKey::TELEMETRY_INTERVAL, config.telemetryInterval);
        setValue(ConfigKey::SENSOR_UPDATE_INTERVAL, config.sensorUpdateInterval);
        setValue(ConfigKey::LCD_UPDATE_INTERVAL, config.lcdUpdateInterval);
        setValue(ConfigKey::SENSOR_PUBLISH_INTERVAL, config.sensorPublishInterval);
    }

    std::optional<SensorConfig> getSensorConfig(size_t index) const {
        if (index >= ConfigConstants::RELAY_COUNT) {
            logger.log("ConfigManager", "ERROR", "Invalid sensor index: " + std::to_string(index));
            return std::nullopt;
        }

        std::shared_lock<std::shared_mutex> lock(mutex);
        SensorConfig config;
        config.threshold = std::get<float>(getValue(ConfigKey::SENSOR_THRESHOLD, index));
        config.activationPeriod = std::get<uint32_t>(getValue(ConfigKey::SENSOR_ACTIVATION_PERIOD, index));
        config.wateringInterval = std::get<uint32_t>(getValue(ConfigKey::SENSOR_WATERING_INTERVAL, index));
        config.sensorEnabled = std::get<bool>(getValue(ConfigKey::SENSOR_ENABLED, index));
        config.relayEnabled = std::get<bool>(getValue(ConfigKey::RELAY_ENABLED, index));
        config.sensorPin = std::get<int>(getValue(ConfigKey::SENSOR_PIN, index));
        config.relayPin = std::get<int>(getValue(ConfigKey::RELAY_PIN, index));

        return config;
    }

    SoftwareConfig getSoftwareConfig() const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        SoftwareConfig config;
        config.tempOffset = std::get<float>(getValue(ConfigKey::TEMP_OFFSET));
        config.telemetryInterval = std::get<uint32_t>(getValue(ConfigKey::TELEMETRY_INTERVAL));
        config.sensorUpdateInterval = std::get<uint32_t>(getValue(ConfigKey::SENSOR_UPDATE_INTERVAL));
        config.lcdUpdateInterval = std::get<uint32_t>(getValue(ConfigKey::LCD_UPDATE_INTERVAL));
        config.sensorPublishInterval = std::get<uint32_t>(getValue(ConfigKey::SENSOR_PUBLISH_INTERVAL));
        return config;
    }

    std::vector<SensorConfig> getEnabledSensorConfigs() const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        std::vector<SensorConfig> enabledConfigs;
        for (size_t i = 0; i < ConfigConstants::RELAY_COUNT; ++i) {
            auto config = getSensorConfig(i);
            if (config && config->sensorEnabled) {
                enabledConfigs.push_back(*config);
            }
        }
        return enabledConfigs;
    }

    void setHardwareConfig(const HardwareConfig& config) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        setValue(ConfigKey::SDA_PIN, config.sdaPin);
        setValue(ConfigKey::SCL_PIN, config.sclPin);
        setValue(ConfigKey::FLOAT_SWITCH_PIN, config.floatSwitchPin);

        for (size_t i = 0; i < ConfigConstants::RELAY_COUNT; i++) {
            setValue(ConfigKey::SENSOR_PIN, config.moistureSensorPins[i], i);
            setValue(ConfigKey::RELAY_PIN, config.relayPins[i], i);
        }

        notifyObservers(ConfigKey::SDA_PIN);
    }

    HardwareConfig getHardwareConfig() const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        HardwareConfig config;
        config.sdaPin = std::get<int>(getValue(ConfigKey::SDA_PIN));
        config.sclPin = std::get<int>(getValue(ConfigKey::SCL_PIN));
        config.floatSwitchPin = std::get<int>(getValue(ConfigKey::FLOAT_SWITCH_PIN));

        for (size_t i = 0; i < ConfigConstants::RELAY_COUNT; i++) {
            config.moistureSensorPins[i] = std::get<int>(getValue(ConfigKey::SENSOR_PIN, i));
            config.relayPins[i] = std::get<int>(getValue(ConfigKey::RELAY_PIN, i));
        }

        return config;
    }

    void addObserver(ConfigKey key, ObserverCallback callback) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        observers[key].push_back(callback);
    }

    void removeObserver(ConfigKey key, const ObserverCallback& callback) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        auto it = observers.find(key);
        if (it != observers.end()) {
            auto& callbacks = it->second;
            callbacks.erase(std::remove_if(callbacks.begin(), callbacks.end(),
                [&callback](const ObserverCallback& cb) {
                    return &cb == &callback;
                }), callbacks.end());
        }
    }

    void resetToDefault() {
        std::unique_lock<std::shared_mutex> lock(mutex);
        preferences.clear();
        notifyObservers(ConfigKey::SENSOR_THRESHOLD);
        logger.log("ConfigManager", "INFO", "Configuration reset to default values");
    }

private:
    Preferences preferences;
    Logger& logger;
    std::map<ConfigKey, std::vector<ObserverCallback>> observers;
    mutable std::shared_mutex mutex;

    static const std::map<ConfigKey, std::string> keyMap;
    static const std::map<ConfigKey, ConfigValue> defaultValues;

    std::string getPreferenceKey(ConfigKey key, size_t index = 0) const {
        auto it = keyMap.find(key);
        if (it != keyMap.end()) {
            const auto& baseKey = it->second;
            if (key >= ConfigKey::SENSOR_THRESHOLD && key <= ConfigKey::RELAY_PIN) {
                return "sensor" + std::to_string(index) + "_" + baseKey;
            }
            return baseKey;
        }
        return ""; // Return empty string for unknown keys
    }

    bool setValueInternal(const std::string& key, const ConfigValue& value) {
        return std::visit([&](auto&& arg) -> bool {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool>) {
                return preferences.putBool(key.c_str(), arg);
            } else if constexpr (std::is_same_v<T, int>) {
                return preferences.putInt(key.c_str(), arg);
            } else if constexpr (std::is_same_v<T, float>) {
                return preferences.putFloat(key.c_str(), arg);
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                return preferences.putULong(key.c_str(), arg);
            }
            return false;
        }, value);
    }

    std::optional<ConfigValue> getValueInternal(const std::string& key) const {
        // We need to remove const for these calls due to Preferences API limitations
        Preferences& mutablePreferences = const_cast<Preferences&>(preferences);

        if (!mutablePreferences.isKey(key.c_str())) {
            return std::nullopt;
        }

        switch (mutablePreferences.getType(key.c_str())) {
            case PT_I32: return mutablePreferences.getInt(key.c_str());
            case PT_U32: return static_cast<uint32_t>(mutablePreferences.getULong(key.c_str()));
            case PT_BLOB: return mutablePreferences.getFloat(key.c_str());
            case PT_U8: return static_cast<bool>(mutablePreferences.getUChar(key.c_str()) != 0);
            default: return std::nullopt;
        }
    }

    void notifyObservers(ConfigKey key) {
        auto it = observers.find(key);
        if (it != observers.end()) {
            for (const auto& callback : it->second) {
                callback(key);
            }
        }
    }
};

// Define the static member outside the class
inline const std::map<ConfigManager::ConfigKey, std::string> ConfigManager::keyMap = {
    {ConfigKey::SENSOR_THRESHOLD, "threshold"},
    {ConfigKey::SENSOR_ACTIVATION_PERIOD, "activationPeriod"},
    {ConfigKey::SENSOR_WATERING_INTERVAL, "wateringInterval"},
    {ConfigKey::SENSOR_ENABLED, "sensorEnabled"},
    {ConfigKey::RELAY_ENABLED, "relayEnabled"},
    {ConfigKey::SENSOR_PIN, "sensorPin"},
    {ConfigKey::RELAY_PIN, "relayPin"},
    {ConfigKey::SDA_PIN, "sdaPin"},
    {ConfigKey::SCL_PIN, "sclPin"},
    {ConfigKey::FLOAT_SWITCH_PIN, "floatSwitchPin"},
    {ConfigKey::TEMP_OFFSET, "tempOffset"},
    {ConfigKey::TELEMETRY_INTERVAL, "telemetryInterval"},
    {ConfigKey::SENSOR_UPDATE_INTERVAL, "sensorUpdateInterval"},
    {ConfigKey::LCD_UPDATE_INTERVAL, "lcdPublishInterval"},
    {ConfigKey::SENSOR_PUBLISH_INTERVAL, "sensorPublishInterval"}    
};

inline const std::map<ConfigManager::ConfigKey, ConfigManager::ConfigValue> ConfigManager::defaultValues = {
    {ConfigKey::SENSOR_THRESHOLD, ConfigConstants::DEFAULT_THRESHOLD},
    {ConfigKey::SENSOR_ACTIVATION_PERIOD, ConfigConstants::DEFAULT_ACTIVATION_PERIOD},
    {ConfigKey::SENSOR_WATERING_INTERVAL, ConfigConstants::DEFAULT_WATERING_INTERVAL},
    {ConfigKey::SENSOR_ENABLED, true},
    {ConfigKey::RELAY_ENABLED, true},
    {ConfigKey::SENSOR_PIN, ConfigConstants::DEFAULT_MOISTURE_SENSOR_PINS[0]},
    {ConfigKey::RELAY_PIN, ConfigConstants::DEFAULT_RELAY_PINS[0]},
    {ConfigKey::SDA_PIN, ConfigConstants::DEFAULT_SDA_PIN},
    {ConfigKey::SCL_PIN, ConfigConstants::DEFAULT_SCL_PIN},
    {ConfigKey::FLOAT_SWITCH_PIN, ConfigConstants::DEFAULT_FLOAT_SWITCH_PIN},
    {ConfigKey::TEMP_OFFSET, ConfigConstants::DEFAULT_TEMP_OFFSET},
    {ConfigKey::TELEMETRY_INTERVAL, ConfigConstants::DEFAULT_TELEMETRY_INTERVAL},
    {ConfigKey::SENSOR_UPDATE_INTERVAL, ConfigConstants::DEFAULT_SENSOR_UPDATE_INTERVAL},
    {ConfigKey::LCD_UPDATE_INTERVAL, ConfigConstants::DEFAULT_LCD_UPDATE_INTERVAL},
    {ConfigKey::SENSOR_PUBLISH_INTERVAL, ConfigConstants::DEFAULT_SENSOR_PUBLISH_INTERVAL}
};

#endif // CONFIG_MANAGER_H