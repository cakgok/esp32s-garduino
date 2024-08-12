#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <mutex>
#include <array>
#include <Adafruit_BMP085.h>
#include "ConfigManager.h"  // Include ConfigManager

struct SensorData {
    std::array<float, 4> moisture;
    float temperature;
    float pressure;
    bool waterLevel;
    unsigned long lastUpdateTime;
};

class SensorManager {
private:
    SensorData data;
    std::mutex dataMutex;
    const unsigned long UPDATE_INTERVAL = 10000; // 10 seconds

    Adafruit_BMP085& bmp;
    const std::array<int, 4>& moistureSensorPins;
    const int floatSwitchPin;
    ConfigManager& configManager;  // Reference to ConfigManager

    static void sensorTaskFunction(void* pvParameters);
    TaskHandle_t sensorTaskHandle;

public:
    SensorManager(Adafruit_BMP085& bmp, 
                  const std::array<int, 4>& moistureSensorPins, 
                  int floatSwitchPin,
                  ConfigManager& configManager)
        : bmp(bmp), 
          moistureSensorPins(moistureSensorPins), 
          floatSwitchPin(floatSwitchPin),
          configManager(configManager) {
        xTaskCreatePinnedToCore(
            sensorTaskFunction,
            "SensorTask",
            4096,
            this,
            1,
            &sensorTaskHandle,
            0
        );
    }

    void updateSensorData();
    SensorData getSensorData();

private:
    float readMoistureSensor(int sensorPin);
    bool checkWaterLevel();
};

#endif // SENSORMANAGER_H