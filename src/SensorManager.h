#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <mutex>
#include <array>
#include <Adafruit_BMP085.h>
#include <LiquidCrystal_I2C.h>
#include "ConfigManager.h"
#include <Wire.h>

struct SensorData {
    std::array<float, 4> moisture;
    float temperature;
    float pressure;
    bool waterLevel;
    std::array<bool, 4> relayState;
};

class SensorManager {
private:
    SensorData data;
    std::mutex dataMutex;
    
    Adafruit_BMP085 bmp;
    std::array<int, 4> moistureSensorPins;
    int floatSwitchPin;
    ConfigManager& configManager;

    static void sensorTaskFunction(void* pvParameters);
    TaskHandle_t sensorTaskHandle;

public:
    SensorManager(ConfigManager& configManager)
        : configManager(configManager), sensorTaskHandle(nullptr) {}

    void setupFloatSwitch(int pin);
    void setupSensors(int sda_pin, int scl_pin);
    void setupMoistureSensors(const std::array<int, 4>& pins);
    void initLCD();
    void updateSensorData();
    SensorData getSensorData();
    void startSensorTask(); // New method to start the sensor task

private:
    float readMoistureSensor(int sensorPin);
    bool checkWaterLevel();
};

#endif // SENSORMANAGER_H