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
#include "PreferencesHandler.h"
#include "ConfigTypes.h"

class ConfigManager {
public:
    ConfigManager(PreferencesHandler& preferencesHandler) : logger(Logger::instance()), prefsHandler(preferencesHandler) {
        preferences.begin("config", false);
        initializeConfig();
    }
                    
    void initializeConfig();

    template<typename T>
    auto get(const ConfigKey key, std::optional<size_t> sensorIndex = std::nullopt) const {
        auto it = combinedConfigMap.find(key);
        if (it == combinedConfigMap.end()) {
            logger.log("CONFIG", LogLevel::ERROR, "Invalid ConfigKey");
            return std::optional<typename ConfigInfo::value_type>();
        }

        const auto& configInfo = it->second;

        if (configMap.find(key) != configMap.end()) {
            if (!sensorIndex.has_value()) {
                logger.log("CONFIG", LogLevel::ERROR, "Sensor index not provided for SensorConfig");
                return std::optional<typename ConfigInfo::value_type>();
            }
            if (sensorIndex.value() >= sensorConfigs.size()) {
                logger.log("CONFIG", LogLevel::ERROR, "Invalid sensor index");
                return std::optional<typename ConfigInfo::value_type>();
            }
        }

        return std::optional<typename ConfigInfo::value_type>(configInfo.defaultValue);
    }
 
    template<typename ConfigType, typename T>
    bool set(ConfigType& config, ConfigKey key, const T& value, std::optional<size_t> sensorIndex = std::nullopt) {
        
        const auto& configInfo = combinedConfigMap.at(key);
        if (combinedConfigMap.find(key) == combinedConfigMap.end()) {
            logger.log("CONFIG", LogLevel::ERROR, "Invalid ConfigKey");
            return false;
        }

        if (!isExpectedType<T>(configInfo)) {
            logger.log("CONFIG", LogLevel::ERROR, "Type mismatch for ConfigKey");
            return false;
        }

        if (!validateRange(configInfo, value)) {
            logger.log("CONFIG", LogLevel::ERROR, "Value out of allowed range");
            return false;
        }

        if constexpr (std::is_same_v<ConfigType, std::vector<SensorConfig>>) {
            if (!sensorIndex.has_value()) {
                logger.log("CONFIG", LogLevel::ERROR, "Sensor index not provided for SensorConfig");
                return false;
            }
            if (sensorIndex.value() >= config.size()) {
                logger.log("CONFIG", LogLevel::ERROR, "Invalid sensor index");
                return false;
            }
            return updateIfChanged(config[sensorIndex.value()], key, value);
        } 
        
        return updateIfChanged(config, key, value);
    }

private:
    mutable std::shared_mutex mutex;
    Preferences preferences;
    PreferencesHandler& prefsHandler;
    Logger& logger;
    static const std::map<ConfigKey, ConfigInfo> configMap;
    std::vector<SensorConfig> sensorConf;
    HardwareConfig hwConf;
    SoftwareConfig swConf;

    template<typename T>
    bool updateIfChanged(ConfigKey key, const T& value) {};

    template<typename T>
    bool validateRange(const ConfigInfo& configInfo, const T& value) {
        if constexpr (std::is_arithmetic_v<T>) {
            if (configInfo.minValue.has_value() && configInfo.maxValue.has_value()) {
                T minVal = std::get<T>(*configInfo.minValue);
                T maxVal = std::get<T>(*configInfo.maxValue);
                return (value >= minVal && value <= maxVal);
            }
        }
        return true;  // If not arithmetic or no range specified, consider it valid
    }

    template<typename T>
    bool isExpectedType(const ConfigInfo& configInfo) {
        return std::holds_alternative<T>(configInfo.defaultValue);
    }
};