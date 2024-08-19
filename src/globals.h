#ifndef GLOBALS_H
#define GLOBALS_H

#include <array>
#include <cstdint>  

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
    constexpr uint32_t DEFAULT_SENSOR_PUBLISH_INTERVAL = 30000; // 30 seconds

    // Hardware pins
    static constexpr size_t RELAY_COUNT = 4;
    constexpr int DEFAULT_SDA_PIN = 21;
    constexpr int DEFAULT_SCL_PIN = 22;
    constexpr int DEFAULT_FLOAT_SWITCH_PIN = 32;
    constexpr std::array<int, 4> DEFAULT_MOISTURE_SENSOR_PINS = {34, 35, 36, 39};
    constexpr std::array<int, 4> DEFAULT_RELAY_PINS = {33, 25, 17, 16};
}

#endif