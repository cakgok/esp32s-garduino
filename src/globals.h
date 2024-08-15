#ifndef GLOBALS_H
#define GLOBALS_H

#define SDA_PIN 21
#define SCL_PIN 22
#define RELAY_COUNT 4
const std::array<int, 4> MOISTURE_SENSOR_PINS = {34, 35, 36, 39};
const int FLOAT_SWITCH_PIN = 32;  // Adjust as needed
const int RELAY_PINS[RELAY_COUNT] = {33, 25, 17, 16}; // Adjust pin numbers as needed

#endif