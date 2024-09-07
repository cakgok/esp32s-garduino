#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <mutex>
#include <shared_mutex>
#include <map>
#include <Adafruit_BMP085.h>
#include "ConfigManager.h"
#include "ESPLogger.h"
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>

struct SensorData {
    std::vector<float> moisture;
    float temperature;
    float pressure;
    bool waterLevel;
};

class SensorManager {
private:
    SensorData data;
    mutable std::shared_mutex dataMutex;
    Adafruit_BMP085 bmp;
    ConfigManager& configManager;
    Logger& logger;
    TaskHandle_t sensorTaskHandle;
    int floatSwitchPin;

    static void sensorTaskFunction(void* pvParameters);
    float readMoistureSensor(int sensorPin);
    bool checkWaterLevel();
    void updateSensorData();
    void sizeMoistureData();
public:
    SensorManager(ConfigManager& configManager);
    void setupFloatSwitch();
    void setupSensors();
    const SensorData& getSensorData() const;
    void startSensorTask();
};

#endif // SENSORMANAGER_H