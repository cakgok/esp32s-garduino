#ifndef CONFIG_TYPES_H
#define CONFIG_TYPES_H

#include <string>
#include <variant>
#include <vector>
#include <optional>
#include <map>

// @note std::optional is used in order to facilitate partial updates of the configguration.
//       This allows us to update only the fields that have changed, without the need to re-send the entire configuration.
namespace ConfigTypes {
    struct HardwareConfig {
        std::optional<int> systemSize;
        std::optional<int> sdaPin;
        std::optional<int> sclPin;
        std::optional<int> floatSwitchPin;
        std::vector<int> moistureSensorPins;
        std::vector<int> relayPins;
    };

    struct SoftwareConfig {
        std::optional<float> tempOffset;
        std::optional<uint32_t> telemetryInterval;
        std::optional<uint32_t> sensorUpdateInterval;
        std::optional<uint32_t> lcdUpdateInterval;
        std::optional<uint32_t> sensorPublishInterval;
    };

    struct SensorConfig {
        std::optional<float> threshold;
        std::optional<uint32_t> activationPeriod;
        std::optional<uint32_t> wateringInterval;
        std::optional<bool> sensorEnabled;
        std::optional<bool> relayEnabled;
    };
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
    SENSOR_PUBLISH_INTERVAL,
    SENSOR_RELAY_MAPPING,
    SYSTEM_SIZE,
};

struct ConfigInfo {
    std::string confType;
    std::string confKey;
    std::string prefKey;
    std::variant<int, float, bool, std::vector<int>, std::vector<bool>, std::vector<int64_t>> defaultValue;
    std::optional<std::variant<int, float>> minValue;
    std::optional<std::variant<int, float>> maxValue;
};

inline const std::map<ConfigKey, ConfigInfo> configMap = {
    {ConfigKey::SENSOR_THRESHOLD, {"sensorConf", "sensorThreshold", "th", 25, 5, 75}},
    {ConfigKey::SENSOR_ACTIVATION_PERIOD, {"sensorConf", "activationPeriod", "ap", 5000, 1000, 60000}},
    {ConfigKey::SENSOR_WATERING_INTERVAL, {"sensorConf", "wateringInterval", "wi", 86400000, 3600000, 604800000}},
    {ConfigKey::SENSOR_ENABLED, {"sensorConf", "sensorEnabled", "se", true, std::nullopt, std::nullopt}},
    {ConfigKey::RELAY_ENABLED, {"sensorConf", "relayEnabled", "re", true, std::nullopt, std::nullopt}},
    {ConfigKey::SENSOR_PIN, {"hwConf", "sensorPin", "sp", std::vector<int>{34, 35, 36, 39}, std::nullopt, std::nullopt}},
    {ConfigKey::RELAY_PIN, {"hwConf", "relayPin", "rp", std::vector<int>{33, 25, 17, 16}, std::nullopt, std::nullopt}},
    {ConfigKey::SDA_PIN, {"hwConf", "sdaPin", "sda", 21, std::nullopt, std::nullopt}},
    {ConfigKey::SCL_PIN, {"hwConf", "sclPin", "scl", 22, std::nullopt, std::nullopt}},
    {ConfigKey::FLOAT_SWITCH_PIN, {"hwConf", "floatSwitchPin", "fsp", 16, std::nullopt, std::nullopt}},
    {ConfigKey::TEMP_OFFSET, {"swConf", "tempOffset", "to", 0.0f, -10.0f, 10.0f}},
    {ConfigKey::TELEMETRY_INTERVAL, {"swConf", "telemetryInterval", "ti", 60000, 10000, 360000}},
    {ConfigKey::SENSOR_UPDATE_INTERVAL, {"swConf", "sensorUpdateInterval", "sui", 60000, 10000, 360000}},
    {ConfigKey::LCD_UPDATE_INTERVAL, {"swConf", "lcdUpdateInterval", "lui", 5000, 10000, 60000}},
    {ConfigKey::SENSOR_PUBLISH_INTERVAL, {"swConf", "sensorPublishInterval", "spi", 60000, 10000, 360000}},
    {ConfigKey::SYSTEM_SIZE, {"hwConf", "systemSize", "size", 4, 1, 16}},
};

#endif // CONFIG_TYPES_H