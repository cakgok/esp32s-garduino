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

    void initializeConfigurations() {
        cachedHardwareConfig = getHardwareConfig();
        cachedSoftwareConfig = getSoftwareConfig();
        for (size_t i = 0; i < ConfigConstants::RELAY_COUNT; ++i) {
            cachedSensorConfigs[i] = getSensorConfig(i).value_or(SensorConfig{
                .threshold = ConfigConstants::DEFAULT_THRESHOLD,
                .activationPeriod = ConfigConstants::DEFAULT_ACTIVATION_PERIOD,
                .wateringInterval = ConfigConstants::DEFAULT_WATERING_INTERVAL,
                .sensorEnabled = true,
                .relayEnabled = true,
                .sensorPin = ConfigConstants::DEFAULT_MOISTURE_SENSOR_PINS[i],
                .relayPin = ConfigConstants::DEFAULT_RELAY_PINS[i]
            });
        }
    }

    const HardwareConfig& getCachedHardwareConfig() const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return cachedHardwareConfig;
    }

    const SoftwareConfig& getCachedSoftwareConfig() const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return cachedSoftwareConfig;
    }

    const SensorConfig& getCachedSensorConfig(size_t index) const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        if (index >= ConfigConstants::RELAY_COUNT) {
            logger.log("ConfigManager", LogLevel::ERROR, "Invalid sensor index: %zu", index);
            static const SensorConfig defaultConfig{};
            return defaultConfig;
        }
        return cachedSensorConfigs[index];
    }   

    [[nodiscard]] ConfigValue getValue(ConfigKey key, size_t index = 0) const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        const auto& info = configMap.at(key);
        if (key == ConfigKey::SENSOR_PIN || key == ConfigKey::RELAY_PIN) {
            std::string prefKey = getPrefKey(info.prefKey, index);
            auto value = getValueInternal(prefKey);
            if (value.has_value()) {
                return std::get<int>(*value);
            } else {
                return std::get<std::array<int, ConfigConstants::RELAY_COUNT>>(info.defaultValue)[index];
            }
        } else {
            std::string prefKey = getPrefKey(info.prefKey);
            auto value = getValueInternal(prefKey);
            return value.value_or(info.defaultValue);
        }
    }

    void setSensorConfig(size_t index, const SensorConfig& config) {
        if (index >= ConfigConstants::RELAY_COUNT) {
            logger.log("ConfigManager", LogLevel::ERROR, "Invalid sensor index: %zu", index);
            return;
        }

        std::unique_lock<std::shared_mutex> lock(mutex);
        SensorConfig& currentConfig = cachedSensorConfigs[index];
        bool configChanged = false;

        configChanged |= updateIfChanged(currentConfig.threshold, config.threshold, ConfigKey::SENSOR_THRESHOLD, index);
        configChanged |= updateIfChanged(currentConfig.activationPeriod, config.activationPeriod, ConfigKey::SENSOR_ACTIVATION_PERIOD, index);
        configChanged |= updateIfChanged(currentConfig.wateringInterval, config.wateringInterval, ConfigKey::SENSOR_WATERING_INTERVAL, index);
        configChanged |= updateIfChanged(currentConfig.sensorEnabled, config.sensorEnabled, ConfigKey::SENSOR_ENABLED, index);
        configChanged |= updateIfChanged(currentConfig.relayEnabled, config.relayEnabled, ConfigKey::RELAY_ENABLED, index);
        configChanged |= updateIfChanged(currentConfig.sensorPin, config.sensorPin, ConfigKey::SENSOR_PIN, index);
        configChanged |= updateIfChanged(currentConfig.relayPin, config.relayPin, ConfigKey::RELAY_PIN, index);

        if (updateIfChanged(currentConfig.sensorPin, config.sensorPin, ConfigKey::SENSOR_PIN, index)) {
            configChanged = true;
            cachedHardwareConfig.moistureSensorPins[index] = config.sensorPin;
        }
        if (updateIfChanged(currentConfig.relayPin, config.relayPin, ConfigKey::RELAY_PIN, index)) {
            configChanged = true;
            cachedHardwareConfig.relayPins[index] = config.relayPin;
        }

        if (configChanged) {
            cachedSensorConfigs[index] = currentConfig;
        }
    }

    void setSoftwareConfig(const SoftwareConfig& config) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        bool configChanged = false;

        configChanged |= updateIfChanged(cachedSoftwareConfig.tempOffset, config.tempOffset, ConfigKey::TEMP_OFFSET);
        configChanged |= updateIfChanged(cachedSoftwareConfig.telemetryInterval, config.telemetryInterval, ConfigKey::TELEMETRY_INTERVAL);
        configChanged |= updateIfChanged(cachedSoftwareConfig.sensorUpdateInterval, config.sensorUpdateInterval, ConfigKey::SENSOR_UPDATE_INTERVAL);
        configChanged |= updateIfChanged(cachedSoftwareConfig.lcdUpdateInterval, config.lcdUpdateInterval, ConfigKey::LCD_UPDATE_INTERVAL);
        configChanged |= updateIfChanged(cachedSoftwareConfig.sensorPublishInterval, config.sensorPublishInterval, ConfigKey::SENSOR_PUBLISH_INTERVAL);
    }

    void setHardwareConfig(const HardwareConfig& config) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        bool configChanged = false;

        configChanged |= updateIfChanged(cachedHardwareConfig.sdaPin, config.sdaPin, ConfigKey::SDA_PIN);
        configChanged |= updateIfChanged(cachedHardwareConfig.sclPin, config.sclPin, ConfigKey::SCL_PIN);
        configChanged |= updateIfChanged(cachedHardwareConfig.floatSwitchPin, config.floatSwitchPin, ConfigKey::FLOAT_SWITCH_PIN);

        for (size_t i = 0; i < ConfigConstants::RELAY_COUNT; ++i) {
            if (updateIfChanged(cachedHardwareConfig.moistureSensorPins[i], config.moistureSensorPins[i], ConfigKey::SENSOR_PIN, i)) {
                configChanged = true;
                cachedSensorConfigs[i].sensorPin = config.moistureSensorPins[i];
            }
            if (updateIfChanged(cachedHardwareConfig.relayPins[i], config.relayPins[i], ConfigKey::RELAY_PIN, i)) {
                configChanged = true;
                cachedSensorConfigs[i].relayPin = config.relayPins[i];
            }
        }
    }

    void resetToDefault() {
        std::unique_lock<std::shared_mutex> lock(mutex);
        preferences.clear();
        initializeDefaultValues();
        initializeConfigurations();
        logger.log("ConfigManager", LogLevel::INFO, "Configuration reset to default values");
    }
    
    void end() {
        std::unique_lock<std::shared_mutex> lock(mutex);
        preferences.end();
    }

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

    std::optional<SensorConfig> getSensorConfig(size_t index) const {
        if (index >= ConfigConstants::RELAY_COUNT) {
            logger.log("ConfigManager", LogLevel::ERROR, "Invalid sensor index:");
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
            } else if constexpr (std::is_same_v<T, std::array<int, ConfigConstants::RELAY_COUNT>>) {
                return false;
            }
            return false;
        }, value);
    }

    std::optional<ConfigValue> getValueInternal(const std::string& key) const {
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

    static const std::map<ConfigKey, Range<float>> floatRanges;
    static const std::map<ConfigKey, Range<uint32_t>> uint32Ranges;

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
        using type = std::conditional_t<
            (Key == ConfigKey::SDA_PIN || Key == ConfigKey::SCL_PIN || Key == ConfigKey::FLOAT_SWITCH_PIN),
            HardwareConfig,
            std::conditional_t<
                (Key == ConfigKey::TEMP_OFFSET || Key == ConfigKey::TELEMETRY_INTERVAL || 
                Key == ConfigKey::SENSOR_UPDATE_INTERVAL || Key == ConfigKey::LCD_UPDATE_INTERVAL || 
                Key == ConfigKey::SENSOR_PUBLISH_INTERVAL),
                SoftwareConfig,
                SensorConfig
            >
        >;
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

// key mapping is needed because pref. library has 15 char limit for keys, inline to make it unique for compilation units
inline const std::map<ConfigManager::ConfigKey, ConfigManager::ConfigInfo> ConfigManager::configMap = {
    {ConfigKey::SENSOR_THRESHOLD, {"th", ConfigConstants::DEFAULT_THRESHOLD, ConfigConstants::MIN_THRESHOLD, ConfigConstants::MAX_THRESHOLD}},
    {ConfigKey::SENSOR_ACTIVATION_PERIOD, {"ap", ConfigConstants::DEFAULT_ACTIVATION_PERIOD, ConfigConstants::MIN_ACTIVATION_PERIOD, ConfigConstants::MAX_ACTIVATION_PERIOD}},
    {ConfigKey::SENSOR_WATERING_INTERVAL, {"wi", ConfigConstants::DEFAULT_WATERING_INTERVAL, ConfigConstants::MIN_WATERING_INTERVAL, ConfigConstants::MAX_WATERING_INTERVAL}},
    {ConfigKey::SENSOR_ENABLED, {"se", true, std::nullopt, std::nullopt}},
    {ConfigKey::RELAY_ENABLED, {"re", true, std::nullopt, std::nullopt}},
    {ConfigKey::SENSOR_PIN, {"sp", ConfigConstants::DEFAULT_MOISTURE_SENSOR_PINS, std::nullopt, std::nullopt}},
    {ConfigKey::RELAY_PIN, {"rp", ConfigConstants::DEFAULT_RELAY_PINS, std::nullopt, std::nullopt}},
    {ConfigKey::SDA_PIN, {"sda", ConfigConstants::DEFAULT_SDA_PIN, std::nullopt, std::nullopt}},
    {ConfigKey::SCL_PIN, {"scl", ConfigConstants::DEFAULT_SCL_PIN, std::nullopt, std::nullopt}},
    {ConfigKey::FLOAT_SWITCH_PIN, {"fsp", ConfigConstants::DEFAULT_FLOAT_SWITCH_PIN, std::nullopt, std::nullopt}},
    {ConfigKey::TEMP_OFFSET, {"to", ConfigConstants::DEFAULT_TEMP_OFFSET, ConfigConstants::MIN_TEMP_OFFSET, ConfigConstants::MAX_TEMP_OFFSET}},
    {ConfigKey::TELEMETRY_INTERVAL, {"ti", ConfigConstants::DEFAULT_TELEMETRY_INTERVAL, ConfigConstants::MIN_TELEMETRY_INTERVAL, ConfigConstants::MAX_TELEMETRY_INTERVAL}},
    {ConfigKey::SENSOR_UPDATE_INTERVAL, {"sui", ConfigConstants::DEFAULT_SENSOR_UPDATE_INTERVAL, ConfigConstants::MIN_SENSORUPDATE_INTERVAL, ConfigConstants::MAX_SENSORUPDATE_INTERVAL}},
    {ConfigKey::LCD_UPDATE_INTERVAL, {"lui", ConfigConstants::DEFAULT_LCD_UPDATE_INTERVAL, ConfigConstants::MIN_LCD_INTERVAL, ConfigConstants::MAX_LCD_INTERVAL}},
    {ConfigKey::SENSOR_PUBLISH_INTERVAL, {"spi", ConfigConstants::DEFAULT_SENSOR_PUBLISH_INTERVAL, ConfigConstants::MIN_SENSOR_PUBLISH_INTERVAL, ConfigConstants::MAX_SENSOR_PUBLISH_INTERVAL}}
};


#endif // CONFIG_MANAGER_H