#include "SensorManager.h"

void SensorManager::sensorTaskFunction(void* pvParameters) {
    SensorManager* manager = static_cast<SensorManager*>(pvParameters);
    while (true) {
        manager->updateSensorData();
        vTaskDelay(pdMS_TO_TICKS(manager->UPDATE_INTERVAL));
    }
}

void SensorManager::updateSensorData() {
    unsigned long currentTime = millis();
    std::lock_guard<std::mutex> lock(dataMutex);

    for (int i = 0; i < 4; i++) {
        data.moisture[i] = readMoistureSensor(moistureSensorPins[i]);
    }
    // Use the latest temperature offset from ConfigManager
    data.temperature = bmp.readTemperature() + configManager.getTemperatureOffset();
    data.pressure = bmp.readPressure() / 100.0F;
    data.waterLevel = checkWaterLevel();
    data.lastUpdateTime = currentTime;

    // Use the latest telemetry interval from ConfigManager
    vTaskDelay(pdMS_TO_TICKS(configManager.getTelemetryInterval()));
}


SensorData SensorManager::getSensorData() {
    std::lock_guard<std::mutex> lock(dataMutex);
    return data;
}

float SensorManager::readMoistureSensor(int sensorPin) {
    const int SAMPLES = 10;
    float sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        sum += analogRead(sensorPin);
        vTaskDelay(pdMS_TO_TICKS(10)); // Short delay between readings
    }
    float average = sum / SAMPLES;
    return map(average, 0, 4095, 0, 100);  // Map to 0-100%
}

bool SensorManager::checkWaterLevel() {
    digitalWrite(floatSwitchPin, HIGH);
    vTaskDelay(pdMS_TO_TICKS(10));
    bool waterLevel = digitalRead(floatSwitchPin);
    digitalWrite(floatSwitchPin, LOW);
    return waterLevel;
}
