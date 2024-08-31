#ifndef CONFIG_TYPES_H
#define CONFIG_TYPES_H

#include <string>
#include <variant>
#include <vector>
#include <optional>
#include <map>

struct SensorConfig {
    float threshold;
    uint32_t activationPeriod;
    uint32_t wateringInterval;
    bool sensorEnabled;
    bool relayEnabled;
};

struct HardwareConfig {
    int sdaPin;
    int sclPin;
    int floatSwitchPin;
    std::vector<int> moistureSensorPins;
    std::vector<int> relayPins;
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
    SENSOR_PUBLISH_INTERVAL,
    SENSOR_RELAY_MAPPING,
    SYSTEM_SIZE,
};

struct ConfigInfo {
    std::string confType;
    std::string prefKey;
    std::variant<int, float, bool, std::vector<int>, std::vector<bool>, std::vector<int64_t>> defaultValue;
    std::optional<std::variant<int, float>> minValue;
    std::optional<std::variant<int, float>> maxValue;
};

inline const std::map<ConfigKey, ConfigInfo> combibedConfigMap = {
    {ConfigKey::SENSOR_THRESHOLD, {"sensorConf","th", 50, 0, 100}},
    {ConfigKey::SENSOR_ACTIVATION_PERIOD, {"sensorConf", "ap", 3600, 60, 86400}},
    {ConfigKey::SENSOR_WATERING_INTERVAL, {"sensorConf", "wi", 86400, 3600, 604800}},
    {ConfigKey::SENSOR_ENABLED, {"sensorConf", "se", true, std::nullopt, std::nullopt}},
    {ConfigKey::RELAY_ENABLED, {"sensorConf", "re", true, std::nullopt, std::nullopt}},
    {ConfigKey::SYSTEM_SIZE, {"global", "ss", 4, 0, 16}},
    {ConfigKey::SENSOR_PIN, {"hwConf", "sp", std::vector<int>{32, 33, 34, 35}, std::nullopt, std::nullopt}},
    {ConfigKey::RELAY_PIN, {"hwConf", "rp", std::vector<int>{16, 17, 18, 19}, std::nullopt, std::nullopt}},
    {ConfigKey::SDA_PIN, {"hwConf", "sda", 21, std::nullopt, std::nullopt}},
    {ConfigKey::SCL_PIN, {"hwConf", "scl", 22, std::nullopt, std::nullopt}},
    {ConfigKey::FLOAT_SWITCH_PIN, {"hwConf", "fsp", 23, std::nullopt, std::nullopt}},
    {ConfigKey::TEMP_OFFSET, {"swConf", "to", 0.0f, -10.0f, 10.0f}},
    {ConfigKey::TELEMETRY_INTERVAL, {"swConf", "ti", 300, 60, 3600}},
    {ConfigKey::SENSOR_UPDATE_INTERVAL, {"swConf", "sui", 60, 10, 3600}},
    {ConfigKey::LCD_UPDATE_INTERVAL, {"swConf", "lui", 5000, 10000, 60000}},
    {ConfigKey::SENSOR_PUBLISH_INTERVAL, {"swConf", "spi", 300, 60, 3600}},
    {ConfigKey::SENSOR_RELAY_MAPPING, {"swConf", "srm", std::vector<int>{}, std::nullopt, std::nullopt}},
};

#endif // CONFIG_TYPES_H

// inline const std::map<ConfigKey, ConfigInfo> configMap = {
//     {ConfigKey::SENSOR_THRESHOLD, {"th", 50, 0, 100}},
//     {ConfigKey::SENSOR_ACTIVATION_PERIOD, {"ap", 3600, 60, 86400}},
//     {ConfigKey::SENSOR_WATERING_INTERVAL, {"wi", 86400, 3600, 604800}},
//     {ConfigKey::SENSOR_ENABLED, {"se", true, std::nullopt, std::nullopt}},
//     {ConfigKey::RELAY_ENABLED, {"re", true, std::nullopt, std::nullopt}},
// };
// inline const std::map<ConfigKey, ConfigInfo> hardwareConfigMap = {
//     {ConfigKey::SYSTEM_SIZE, {"ss", 4, 0, 16}},
//     {ConfigKey::SENSOR_PIN, {"sp", std::vector<int>{32, 33, 34, 35}, std::nullopt, std::nullopt}},
//     {ConfigKey::RELAY_PIN, {"rp", std::vector<int>{16, 17, 18, 19}, std::nullopt, std::nullopt}},
//     {ConfigKey::SDA_PIN, {"sda", 21, std::nullopt, std::nullopt}},
//     {ConfigKey::SCL_PIN, {"scl", 22, std::nullopt, std::nullopt}},
//     {ConfigKey::FLOAT_SWITCH_PIN, {"fsp", 23, std::nullopt, std::nullopt}},
// };
// inline const std::map<ConfigKey, ConfigInfo> globalConfigMap = {
//     {ConfigKey::TEMP_OFFSET, {"to", 0.0f, -10.0f, 10.0f}},
//     {ConfigKey::TELEMETRY_INTERVAL, {"ti", 300, 60, 3600}},
//     {ConfigKey::SENSOR_UPDATE_INTERVAL, {"sui", 60, 10, 3600}},
//     {ConfigKey::LCD_UPDATE_INTERVAL, {"lui", 5000, 10000, 60000}},
//     {ConfigKey::SENSOR_PUBLISH_INTERVAL, {"spi", 300, 60, 3600}},
//     {ConfigKey::SENSOR_RELAY_MAPPING, {"srm", std::vector<int>{}, std::nullopt, std::nullopt}},
// };