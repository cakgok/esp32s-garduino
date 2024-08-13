#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Preferences.h>
#include <map>
#include <string>
#include <functional>
#include <array>
#include <vector>
#include <cstdint>
#include <optional>

namespace ConfigConstants {
    constexpr uint32_t MIN_TELEMETRY_INTERVAL = 10000;  // 10 seconds
    constexpr uint32_t MAX_TELEMETRY_INTERVAL = 600000; // 10 minutes
    constexpr uint32_t MIN_SENSORUPDATE_INTERVAL = 10000;  // 10 seconds
    constexpr uint32_t MAX_SENSORUPDATE_INTERVAL = 600000; // 10 minutes
    constexpr uint32_t MIN_LCD_INTERVAL = 5000;  // 5 seconds
    constexpr uint32_t MAX_LCD_INTERVAL = 60000; // 1 minute
    constexpr float MIN_TEMP_OFFSET = -5.0f;
    constexpr float MAX_TEMP_OFFSET = 5.0f;
    constexpr uint32_t MIN_ACTIVATION_PERIOD = 1000;   // 1 second
    constexpr uint32_t MAX_ACTIVATION_PERIOD = 60000;  // 60 seconds
    constexpr float MIN_THRESHOLD = 5.0f;
    constexpr float MAX_THRESHOLD = 50.0f;
    constexpr uint32_t MIN_WATERING_INTERVAL = 3600000;   // 1 hour
    constexpr uint32_t MAX_WATERING_INTERVAL = 432000000; // 120 hours
    constexpr uint32_t MIN_SENSOR_PUBLISH_INTERVAL = 10000;
    constexpr uint32_t MAX_SENSOR_PUBLISH_INTERVAL = 600000;

    // Default values
    constexpr float DEFAULT_TEMP_OFFSET = 0.0f;
    constexpr uint32_t DEFAULT_TELEMETRY_INTERVAL = 60000; // 1 minute
    constexpr float DEFAULT_THRESHOLD = 25.0f;
    constexpr uint32_t DEFAULT_ACTIVATION_PERIOD = 30000; // 30 seconds
    constexpr uint32_t DEFAULT_WATERING_INTERVAL = 86400000; // 24 hours
    constexpr uint32_t DEFAULT_SENSOR_UPDATE_INTERVAL = 10000; // 10 seconds
    constexpr uint32_t DEFAULT_LCD_UPDATE_INTERVAL = 5000; // 5 seconds
    constexpr uint32_t DEFAULT_SENSOR_PUBLISH_INTERVAL = 100000; //  seconds
}

class ConfigManager {
public:
    static constexpr size_t RELAY_COUNT = 4;
    
    struct SensorConfig {
        float threshold;
        uint32_t activationPeriod;
        uint32_t wateringInterval;
    };

    struct CachedConfig {
        std::optional<float> temperatureOffset;
        std::optional<uint32_t> telemetryInterval;
        std::optional<uint32_t> sensorUpdateInterval;
        std::optional<uint32_t> sensorPublishInterval;
        std::optional<uint32_t> lcdUpdateInterval;
        std::array<std::optional<SensorConfig>, RELAY_COUNT> sensorConfigs;
    };

    using ObserverCallback = std::function<void(const std::string&)>;

    ConfigManager() : preferences() {}

    bool begin(const char* name = "irrigation-config") {
        bool success = preferences.begin(name, false);
        if (success) {
            loadCachedValues();
        }
        return success;
    }

    void end() {
        preferences.end();
    }

    // Endpoint for saving changes from the web interface
    bool updateAndSaveAll(
        std::optional<float> tempOffset,
        std::optional<uint32_t> telemetryInterval,
        std::optional<uint32_t> sensorUpdateInterval,
        std::optional<uint32_t> lcdUpdateInterval,
        std::optional<uint32_t> sensorPublishInterval,
        const std::array<std::optional<SensorConfig>, RELAY_COUNT>& sensorConfigs) 
    {
        bool isValid = true;
        
        if (tempOffset) isValid &= setTemperatureOffset(*tempOffset);
        if (telemetryInterval) isValid &= setTelemetryInterval(*telemetryInterval);
        if (sensorUpdateInterval) isValid &= setSensorUpdateInterval(*sensorUpdateInterval);
        if (lcdUpdateInterval) isValid &= setLCDUpdateInterval(*lcdUpdateInterval);
        if (sensorPublishInterval) isValid &= setSensorPublishInterval(*sensorPublishInterval);

        for (size_t i = 0; i < RELAY_COUNT; i++) {
            if (sensorConfigs[i]) isValid &= setSensorConfig(i, *sensorConfigs[i]);
        }
        
        if (isValid) {
            return saveChanges();
        }
        
        return false;
    }

    bool setLCDUpdateInterval(uint32_t interval) {
        if (isInRange(interval, ConfigConstants::MIN_LCD_INTERVAL, ConfigConstants::MAX_LCD_INTERVAL)) {
            cachedConfig.lcdUpdateInterval = interval;
            notifyObservers("lcdUpdateInterval");
            return true;
        }
        return false;
    }

    bool setSensorUpdateInterval(uint32_t interval) {
        if (isInRange(interval, ConfigConstants::MIN_SENSORUPDATE_INTERVAL, ConfigConstants::MAX_SENSORUPDATE_INTERVAL)) {
                cachedConfig.sensorUpdateInterval = interval;
                notifyObservers("sensorUpdateInterval");
                return true;
            return true;
        }
        return false;
    }

    bool setTemperatureOffset(float offset) {
        if (isInRange(offset, ConfigConstants::MIN_TEMP_OFFSET, ConfigConstants::MAX_TEMP_OFFSET)) {
            cachedConfig.temperatureOffset = offset;
            notifyObservers("temperatureOffset");
            return true;
        }
        return false;
    }

    bool setTelemetryInterval(uint32_t interval) {
        if (isInRange(interval, ConfigConstants::MIN_TELEMETRY_INTERVAL, ConfigConstants::MAX_TELEMETRY_INTERVAL)) {
            cachedConfig.telemetryInterval = interval;
            notifyObservers("telemetryInterval");
            return true;
        }
        return false;
    }

    bool setSensorConfig(size_t index, const SensorConfig& config) {
        if (index < RELAY_COUNT &&
            isInRange(config.threshold, ConfigConstants::MIN_THRESHOLD, ConfigConstants::MAX_THRESHOLD) &&
            isInRange(config.activationPeriod, ConfigConstants::MIN_ACTIVATION_PERIOD, ConfigConstants::MAX_ACTIVATION_PERIOD) &&
            isInRange(config.wateringInterval, ConfigConstants::MIN_WATERING_INTERVAL, ConfigConstants::MAX_WATERING_INTERVAL)) {
            cachedConfig.sensorConfigs[index] = config;
            notifyObservers("sensorConfig_" + std::to_string(index));
            return true;
        }
        return false;
    }

    bool setSensorPublishInterval(uint32_t interval) {
        if (isInRange(interval, ConfigConstants::MIN_SENSOR_PUBLISH_INTERVAL, ConfigConstants::MAX_SENSOR_PUBLISH_INTERVAL)) {
            cachedConfig.sensorPublishInterval = interval;
            notifyObservers("sensorPublishInterval");
            return true;
        }
        return false;
    }

    // Specific Getters
    float getTemperatureOffset() const { 
        return cachedConfig.temperatureOffset.value_or(ConfigConstants::DEFAULT_TEMP_OFFSET);
    }

    uint32_t getTelemetryInterval() const { 
        return cachedConfig.telemetryInterval.value_or(ConfigConstants::DEFAULT_TELEMETRY_INTERVAL);
    }

    uint32_t getSensorUpdateInterval() const { 
        return cachedConfig.sensorUpdateInterval.value_or(ConfigConstants::DEFAULT_SENSOR_UPDATE_INTERVAL);
    }

    uint32_t getLCDUpdateInterval() const { 
        return cachedConfig.lcdUpdateInterval.value_or(ConfigConstants::DEFAULT_LCD_UPDATE_INTERVAL);
    }

    uint32_t getSensorPublishInterval() const {
        return cachedConfig.sensorPublishInterval.value_or(ConfigConstants::DEFAULT_SENSOR_PUBLISH_INTERVAL);
    }

    SensorConfig getSensorConfig(size_t index) const { 
        if (index < RELAY_COUNT) {
            return cachedConfig.sensorConfigs[index].value_or(SensorConfig{
                ConfigConstants::DEFAULT_THRESHOLD,
                ConfigConstants::DEFAULT_ACTIVATION_PERIOD,
                ConfigConstants::DEFAULT_WATERING_INTERVAL
            });
        }
        return SensorConfig{
            ConfigConstants::DEFAULT_THRESHOLD,
            ConfigConstants::DEFAULT_ACTIVATION_PERIOD,
            ConfigConstants::DEFAULT_WATERING_INTERVAL
        };
    }

    // Observer pattern methods
    void addObserver(const std::string& key, ObserverCallback callback) {
        observers[key].push_back(callback);
    }

    void removeObserver(const std::string& key, const ObserverCallback& callback) {
        auto it = observers.find(key);
        if (it != observers.end()) {
            auto& callbacks = it->second;
            callbacks.erase(std::remove_if(callbacks.begin(), callbacks.end(),
                [&callback](const ObserverCallback& cb) {
                    return &cb == &callback;
                }), callbacks.end());
        }
    }

    bool resetToDefault() {
        cachedConfig = CachedConfig();
        bool success = saveChanges();
        if (success) {
            notifyObservers("all");
        }
        return success;
    }
    
    // Provide read-only access to cachedConfig
    const CachedConfig& getCachedConfig() const {
        return cachedConfig;
    }

private:
    Preferences preferences;
    CachedConfig cachedConfig;
    std::map<std::string, std::vector<ObserverCallback>> observers;

    void loadCachedValues() {
        cachedConfig.temperatureOffset = preferences.getFloat("tempOffset", ConfigConstants::DEFAULT_TEMP_OFFSET);
        cachedConfig.telemetryInterval = preferences.getULong("telemetryInterval", ConfigConstants::DEFAULT_TELEMETRY_INTERVAL);
        cachedConfig.sensorUpdateInterval = preferences.getULong("sensorUpdateInterval", ConfigConstants::DEFAULT_SENSOR_UPDATE_INTERVAL);
        cachedConfig.lcdUpdateInterval = preferences.getULong("lcdUpdateInterval", ConfigConstants::DEFAULT_LCD_UPDATE_INTERVAL);
        cachedConfig.sensorPublishInterval = preferences.getULong("sensorPublishInterval", ConfigConstants::DEFAULT_SENSOR_PUBLISH_INTERVAL);
        for (size_t i = 0; i < RELAY_COUNT; i++) {
            loadSensorConfig(i);
        }
    }

    void loadSensorConfig(size_t index) {
        std::string prefix = "sensor" + std::to_string(index) + "_";
        SensorConfig config;
        config.threshold = preferences.getFloat((prefix + "threshold").c_str(), ConfigConstants::DEFAULT_THRESHOLD);
        config.activationPeriod = preferences.getULong((prefix + "activationPeriod").c_str(), ConfigConstants::DEFAULT_ACTIVATION_PERIOD);
        config.wateringInterval = preferences.getULong((prefix + "wateringInterval").c_str(), ConfigConstants::DEFAULT_WATERING_INTERVAL);
        cachedConfig.sensorConfigs[index] = config;
    }

     bool saveSensorConfig(size_t index, const SensorConfig& config) {
        std::string prefix = "sensor" + std::to_string(index) + "_";
        bool success = true;
        success &= preferences.putFloat((prefix + "threshold").c_str(), config.threshold);
        success &= preferences.putULong((prefix + "activationPeriod").c_str(), config.activationPeriod);
        success &= preferences.putULong((prefix + "wateringInterval").c_str(), config.wateringInterval);
        return success;
    }


    bool saveChanges() {
        bool success = true;
        if (cachedConfig.temperatureOffset) {
            success &= preferences.putFloat("tempOffset", *cachedConfig.temperatureOffset);
        }
        if (cachedConfig.telemetryInterval) {
            success &= preferences.putULong("telemetryInterval", *cachedConfig.telemetryInterval);
        }
        if (cachedConfig.sensorUpdateInterval) {
            success &= preferences.putULong("sensorUpdateInterval", *cachedConfig.sensorUpdateInterval);
        }
        if (cachedConfig.lcdUpdateInterval) {
            success &= preferences.putULong("lcdUpdateInterval", *cachedConfig.lcdUpdateInterval);
        }
        
        if (cachedConfig.sensorPublishInterval) {
            success &= preferences.putULong("sensorPublishInterval", *cachedConfig.sensorPublishInterval);
        }

        for (size_t i = 0; i < RELAY_COUNT; i++) {
            if (cachedConfig.sensorConfigs[i]) {
                success &= saveSensorConfig(i, *cachedConfig.sensorConfigs[i]);
            }
        }

        if (success) {
            cachedConfig = CachedConfig();  // Clear all optionals after successful save
            notifyObservers("all");  // Notify that all values have been saved
        }
        return success;
    }

    void notifyObservers(const std::string& key) {
        auto it = observers.find(key);
        if (it != observers.end()) {
            for (const auto& callback : it->second) {
                callback(key);
            }
        }
        // Also notify "all" observers
        it = observers.find("all");
        if (it != observers.end()) {
            for (const auto& callback : it->second) {
                callback(key);
            }
        }
    }

    template<typename T>
    bool isInRange(T value, T min, T max) {
        return value >= min && value <= max;
    }

};

#endif // CONFIG_MANAGER_H