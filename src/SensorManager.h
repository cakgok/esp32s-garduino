#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <mutex>
#include <map>
#include <Adafruit_BMP085.h>
#include "ConfigManager.h"
#include <Wire.h>

struct SensorData {
    std::map<int, float> moisture;  // Key is the sensor pin
    float temperature;
    float pressure;
    bool waterLevel;
    std::map<int, bool> relayState;  // Key is the relay pin
};

class SensorManager {
private:
    SensorData data;
    std::mutex dataMutex;
    
    Adafruit_BMP085 bmp;
    ConfigManager& configManager;

    static void sensorTaskFunction(void* pvParameters);
    TaskHandle_t sensorTaskHandle;

    int floatSwitchPin;

public:
    SensorManager(ConfigManager& configManager)
        : configManager(configManager), sensorTaskHandle(nullptr) {}

    void setupFloatSwitch();
    void setupSensors();
    void updateSensorData();
    SensorData getSensorData();
    void startSensorTask();

private:
    float readMoistureSensor(int sensorPin);
    bool checkWaterLevel();
};

#endif // SENSORMANAGER_H