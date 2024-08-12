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

    // Default values
    constexpr float DEFAULT_TEMP_OFFSET = 0.0f;
    constexpr uint32_t DEFAULT_TELEMETRY_INTERVAL = 60000; // 1 minute
    constexpr float DEFAULT_THRESHOLD = 25.0f;
    constexpr uint32_t DEFAULT_ACTIVATION_PERIOD = 30000; // 30 seconds
    constexpr uint32_t DEFAULT_WATERING_INTERVAL = 86400000; // 24 hours
    constexpr uint32_t DEFAULT_SENSOR_UPDATE_INTERVAL = 10000; // 10 seconds
    constexpr uint32_t DEFAULT_LCD_UPDATE_INTERVAL = 5000; // 5 seconds
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
        float temperatureOffset;
        uint32_t telemetryInterval;
        uint32_t sensorUpdateInterval;
        uint32_t lcdUpdateInterval;
        std::array<SensorConfig, RELAY_COUNT> sensorConfigs;
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

    bool setLCDUpdateInterval(uint32_t interval) {
        if (interval >= ConfigConstants::MIN_LCD_INTERVAL && interval <= ConfigConstants::MAX_LCD_INTERVAL) {
            if (cachedConfig.lcdUpdateInterval != interval) {
                cachedConfig.lcdUpdateInterval = interval;
                dirtyFlags.lcdUpdateInterval = true;
                notifyObservers("lcdUpdateInterval");
            }
            return true;
        }
        return false;
    }

    bool setSensorUpdateInterval(uint32_t interval) {
        if (interval >= ConfigConstants::MIN_SENSORUPDATE_INTERVAL && interval <= ConfigConstants::MAX_SENSORUPDATE_INTERVAL) {
            if (cachedConfig.sensorUpdateInterval != interval) {
                cachedConfig.sensorUpdateInterval = interval;
                dirtyFlags.sensorUpdateInterval = true;
                notifyObservers("sensorUpdateInterval"); 
            }
            return true;
        }
        return false;
    }

    bool setTemperatureOffset(float offset) {
        if (offset >= ConfigConstants::MIN_TEMP_OFFSET && offset <= ConfigConstants::MAX_TEMP_OFFSET) {
            if (cachedConfig.temperatureOffset != offset) {
                cachedConfig.temperatureOffset = offset;
                dirtyFlags.temperatureOffset = true;
                notifyObservers("temperatureOffset");
            }
            return true;
        }
        return false;
    }

    bool setTelemetryInterval(uint32_t interval) {
        if (interval >= ConfigConstants::MIN_TELEMETRY_INTERVAL && interval <= ConfigConstants::MAX_TELEMETRY_INTERVAL) {
            if (cachedConfig.telemetryInterval != interval) {
                cachedConfig.telemetryInterval = interval;
                dirtyFlags.telemetryInterval = true;
                notifyObservers("telemetryInterval");
            }
            return true;
        }
        return false;
    }

    bool setSensorConfig(size_t index, const SensorConfig& config) {
        if (index < RELAY_COUNT &&
            config.threshold >= ConfigConstants::MIN_THRESHOLD && config.threshold <= ConfigConstants::MAX_THRESHOLD &&
            config.activationPeriod >= ConfigConstants::MIN_ACTIVATION_PERIOD && config.activationPeriod <= ConfigConstants::MAX_ACTIVATION_PERIOD &&
            config.wateringInterval >= ConfigConstants::MIN_WATERING_INTERVAL && config.wateringInterval <= ConfigConstants::MAX_WATERING_INTERVAL) {
            cachedConfig.sensorConfigs[index] = config;
            dirtyFlags.sensorConfigs[index] = true;
            notifyObservers("sensorConfig_" + std::to_string(index));
            return true;
        }
        return false;
    } 
    
    // Specific Getters
    float getTemperatureOffset() const { return cachedConfig.temperatureOffset; }
    float getTelemetryInterval() const { return cachedConfig.telemetryInterval; }
    float getSensorUpdateInterval() const { return cachedConfig.sensorUpdateInterval; }
    float getLCDUpdateInterval() const { return cachedConfig.lcdUpdateInterval; }
    const SensorConfig& getSensorConfig(size_t index) const { return cachedConfig.sensorConfigs[index]; }
    
    // Endpoint for saving changes from the web interface
    bool updateAndSaveAll(
        bool updateTempOffset, float tempOffset,
        bool updateTelemetryInterval, uint32_t telemetryInterval,
        bool updateSensorUpdateInterval, uint32_t sensorUpdateInterval,
        const std::array<bool, RELAY_COUNT>& updateSensorConfigs,
        const std::array<SensorConfig, RELAY_COUNT>& sensorConfigs) 
    {
        bool isValid = true;
        bool anyChange = false;
        
        if (updateTempOffset) {
            isValid &= setTemperatureOffset(tempOffset);
            anyChange = true;
        }
        
        if (updateTelemetryInterval) {
            isValid &= setTelemetryInterval(telemetryInterval);
            anyChange = true;
        }
        
        if (updateSensorUpdateInterval) {
            isValid &= setSensorUpdateInterval(sensorUpdateInterval);
            anyChange = true;
        }

        for (size_t i = 0; i < RELAY_COUNT; i++) {
            if (updateSensorConfigs[i]) {
                isValid &= setSensorConfig(i, sensorConfigs[i]);
                anyChange = true;
            }
        }
        // Only save if there were changes and all updates are valid
        if (anyChange && isValid) {
            return saveChanges();
        }
        
        return isValid;
    }

    // Observer pattern methods
    void addObserver(const std::string& key, ObserverCallback callback) {
        observers[key].push_back(callback);
    }

    void removeObserver(const std::string& key, ObserverCallback callback) {
        auto it = observers.find(key);
        if (it != observers.end()) {
            auto& callbacks = it->second;
            callbacks.erase(std::remove_if(callbacks.begin(), callbacks.end(),
                [&callback](const ObserverCallback& cb) {
                    return cb.target_type() == callback.target_type();
                }), callbacks.end());
        }
    }

    bool resetToDefault() {
        CachedConfig defaultConfig;
        defaultConfig.temperatureOffset = ConfigConstants::DEFAULT_TEMP_OFFSET;
        defaultConfig.telemetryInterval = ConfigConstants::DEFAULT_TELEMETRY_INTERVAL;
        defaultConfig.sensorUpdateInterval = ConfigConstants::DEFAULT_SENSOR_UPDATE_INTERVAL;

        for (auto& sensorConfig : defaultConfig.sensorConfigs) {
            sensorConfig.threshold = ConfigConstants::DEFAULT_THRESHOLD;
            sensorConfig.activationPeriod = ConfigConstants::DEFAULT_ACTIVATION_PERIOD;
            sensorConfig.wateringInterval = ConfigConstants::DEFAULT_WATERING_INTERVAL;
        }

        cachedConfig = defaultConfig;
        return saveChanges();
    }

private:
    Preferences preferences;
    CachedConfig cachedConfig;
    std::map<std::string, std::vector<ObserverCallback>> observers;

    struct DirtyFlags {
        bool temperatureOffset = false;
        bool telemetryInterval = false;
        bool sensorUpdateInterval = false;
        bool lcdUpdateInterval = false;
        std::array<bool, RELAY_COUNT> sensorConfigs = {false};
    } dirtyFlags;

    void loadCachedValues() {
        cachedConfig.temperatureOffset = preferences.getFloat("tempOffset", ConfigConstants::DEFAULT_TEMP_OFFSET);
        cachedConfig.telemetryInterval = preferences.getULong("telemetryInterval", ConfigConstants::DEFAULT_TELEMETRY_INTERVAL);
        cachedConfig.sensorUpdateInterval = preferences.getULong("sensorUpdateInterval", ConfigConstants::DEFAULT_SENSOR_UPDATE_INTERVAL);
        cachedConfig.lcdUpdateInterval = preferences.getULong("lcdUpdateInterval", ConfigConstants::DEFAULT_LCD_UPDATE_INTERVAL);
        for (size_t i = 0; i < RELAY_COUNT; i++) {
            loadSensorConfig(i);
        }
    }

    void loadSensorConfig(size_t index) {
        std::string prefix = "sensor" + std::to_string(index) + "_";
        cachedConfig.sensorConfigs[index].threshold = preferences.getFloat((prefix + "threshold").c_str(), ConfigConstants::DEFAULT_THRESHOLD);
        cachedConfig.sensorConfigs[index].activationPeriod = preferences.getULong((prefix + "activationPeriod").c_str(), ConfigConstants::DEFAULT_ACTIVATION_PERIOD);
        cachedConfig.sensorConfigs[index].wateringInterval = preferences.getULong((prefix + "wateringInterval").c_str(), ConfigConstants::DEFAULT_WATERING_INTERVAL);
        cachedConfig.sensorConfigs[index].wateringInterval = preferences.getULong((prefix + "wateringInterval").c_str(), ConfigConstants::DEFAULT_LCD_UPDATE_INTERVAL);
    }


    bool saveSensorConfig(size_t index) {
        std::string prefix = "sensor" + std::to_string(index) + "_";
        bool success = true;
        success &= preferences.putFloat((prefix + "threshold").c_str(), cachedConfig.sensorConfigs[index].threshold);
        success &= preferences.putULong((prefix + "activationPeriod").c_str(), cachedConfig.sensorConfigs[index].activationPeriod);
        success &= preferences.putULong((prefix + "wateringInterval").c_str(), cachedConfig.sensorConfigs[index].wateringInterval);
        return success;
    }

    bool saveChanges() {
        bool success = true;
        if (dirtyFlags.temperatureOffset) {
            success &= preferences.putFloat("tempOffset", cachedConfig.temperatureOffset);
            dirtyFlags.temperatureOffset = false;
        }
        if (dirtyFlags.telemetryInterval) {
            success &= preferences.putULong("telemetryInterval", cachedConfig.telemetryInterval);
            dirtyFlags.telemetryInterval = false;
        }
        if (dirtyFlags.sensorUpdateInterval) {
            success &= preferences.putULong("sensorUpdateInterval", cachedConfig.sensorUpdateInterval);
            dirtyFlags.sensorUpdateInterval = false;
        }
        if (dirtyFlags.lcdUpdateInterval) {
            success &= preferences.putULong("lcdUpdateInterval", cachedConfig.lcdUpdateInterval);
            dirtyFlags.lcdUpdateInterval = false;
        }
        
        for (size_t i = 0; i < RELAY_COUNT; i++) {
            if (dirtyFlags.sensorConfigs[i]) {
                success &= saveSensorConfig(i);
                dirtyFlags.sensorConfigs[i] = false;
            }
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
    }

    template<typename T>
    bool isInRange(T value, T min, T max) {
        return value >= min && value <= max;
    }

};

#endif // CONFIG_MANAGER_H