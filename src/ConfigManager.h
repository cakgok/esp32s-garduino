// Possible improvements:
    // Maybe think of splitting the class. But it's easy to reason about the current structure..
    // ConfigStorage Class: Handle the low-level storage operations (reading/writing to Preferences).
    // ConfigValidator Class: Handle validation of configuration values.
    // ConfigCache Class: Manage the caching of configuration values.
    // ConfigObserver Class: Handle the observer pattern for configuration changes.
    // ConfigManager Class: Act as a facade, coordinating the above classes.
// getValue, setValue, getValueInternal, and setValueInternal methods have some duplication. A template method could potentially handle all types.
// ConfigType struct and getConfigObject function could potentially be replaced with a more straightforward approach using if-constexpr or a map of functions.
// SensorConfig, HardwareConfig, and SoftwareConfig structs could potentially be generated from the configMap to reduce duplication
// setSensorConfig, setSoftwareConfig, and setHardwareConfig methods have similar patterns. A template method could potentially handle all types.

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

class ConfigManager {
public:

    using ConfigValue = std::variant<bool, int, float, uint32_t, std::array<int, ConfigConstants::RELAY_COUNT>>;

    struct ConfigInfo {
        std::string prefKey;
        ConfigValue defaultValue;
        std::optional<ConfigValue> minValue;
        std::optional<ConfigValue> maxValue;
    };

    //for input validation
    template<typename T>
    struct Range {
        T min;
        T max;
    };

    using ObserverCallback = std::function<void(ConfigKey)>;
    ConfigManager() : logger(Logger::instance()),
                    preferences() {}

    
    bool begin(const char* name = "cf") {
        bool result = preferences.begin(name, false);
        if (result) {
            if (!preferences.isKey("initialized")) {
                logger.log("ConfigManager", LogLevel::INFO, "First run, initializing default values");
                initializeDefaultValues();
                preferences.putBool("initialized", true);
            } else {
                logger.log("ConfigManager", LogLevel::INFO, "Config already initialized");
            }
        }
        initializeConfigurations();
        return result;
    }

    // @initializeDefaultsValues is called to fill the pref. library with defaults.
    // @initializeConfigurations is called to fill the cache so that other classes can access it.
    void initializeConfigurations() {}
    const HardwareConfig& getCachedHardwareConfig() const {}
    const SoftwareConfig& getCachedSoftwareConfig() const {}
    const SensorConfig& getCachedSensorConfig(size_t index) const{}
    ConfigValue getValue(ConfigKey key, size_t index = 0) const {}
    void setSensorConfig(size_t index, const SensorConfig& config) {}
    void setSoftwareConfig(const SoftwareConfig& config) {}
    void setHardwareConfig(const HardwareConfig& config) {}

private:
    Preferences preferences;
    Logger& logger;
    std::map<ConfigKey, std::vector<ObserverCallback>> observers;
    mutable std::shared_mutex mutex;
    
    HardwareConfig cachedHardwareConfig;
    SoftwareConfig cachedSoftwareConfig;
    std::array<SensorConfig, ConfigConstants::RELAY_COUNT> cachedSensorConfigs;

    static const std::map<ConfigKey, ConfigInfo> configMap;

    std::string getPrefKey(const std::string& baseKey, size_t index = 0) const {
        if (index > 0) {
            return baseKey + "_" + std::to_string(index);
        }
        return baseKey;
    }

    void initializeDefaultValues() {
        for (const auto& [key, info] : configMap) {
            if (key == ConfigKey::SENSOR_PIN || key == ConfigKey::RELAY_PIN) {
                const auto& pinArray = std::get<std::array<int, ConfigConstants::RELAY_COUNT>>(info.defaultValue);
                for (size_t i = 0; i < ConfigConstants::RELAY_COUNT; ++i) {
                    std::string prefKey = getPrefKey(info.prefKey, i);
                    if (!preferences.isKey(prefKey.c_str())) {
                        setValueInternal(prefKey, pinArray[i]);
                        logger.log("ConfigManager", LogLevel::DEBUG, "Initialized %s %zu to %d", info.prefKey.c_str(), i, pinArray[i]);
                    }
                }
            } else {
                std::string prefKey = getPrefKey(info.prefKey);
                if (!preferences.isKey(prefKey.c_str())) {
                    setValueInternal(prefKey, info.defaultValue);
                }
            }
        }
    }

    // Getters for the preferences library values are kept private, we use cached values in public instead
    SoftwareConfig getSoftwareConfig() const {}
    HardwareConfig getHardwareConfig() const {}
    std::optional<SensorConfig> getSensorConfig(size_t index) const {}
    bool setValueInternal(const std::string& key, const ConfigValue& value) {}
    std::optional<ConfigValue> getValueInternal(const std::string& key) const {}

    template<typename T>
    bool validateValue(ConfigKey key, const T& value) const {
        const auto& info = configMap.at(key);
        if (info.minValue && info.maxValue) {
            return value >= std::get<T>(*info.minValue) && value <= std::get<T>(*info.maxValue);
        }
        return true;
    }

    template<ConfigKey Key>
    struct ConfigType {

    };

    template<typename T>
    bool updateIfChanged(T& currentValue, const T& newValue, ConfigKey key, size_t index = 0) {
        if (currentValue == newValue) return false;
        if (validateValue(key, newValue)) {
            currentValue = newValue;
            std::string prefKey = getPrefKey(configMap.at(key).prefKey, index);
            setValueInternal(prefKey, ConfigValue(newValue));
            return true;
        } else {
            logger.log("ConfigManager", LogLevel::ERROR, "Invalid value for key: %zu", key);
            return false;
        }
        return false;
    }

    // Helper function to get the appropriate config object
    template<ConfigKey Key>
    typename ConfigType<Key>::type& getConfigObject(size_t index = 0) {
        if constexpr (std::is_same_v<typename ConfigType<Key>::type, HardwareConfig>) {
            return cachedHardwareConfig;
        } else if constexpr (std::is_same_v<typename ConfigType<Key>::type, SoftwareConfig>) {
            return cachedSoftwareConfig;
        } else {
            return cachedSensorConfigs[index];
        }
    }
};

#endif // CONFIG_MANAGER_H