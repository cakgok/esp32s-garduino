#include "SensorManager.h"

void SensorManager::sensorTaskFunction(void* pvParameters) {
    SensorManager* manager = static_cast<SensorManager*>(pvParameters);
    while (true) {
        manager->updateSensorData();
        uint32_t currentInterval = manager->configManager.getSensorUpdateInterval();
        vTaskDelay(pdMS_TO_TICKS(currentInterval));
    }
}

void SensorManager::setupFloatSwitch(int pin) {
    floatSwitchPin = pin;
    pinMode(floatSwitchPin, INPUT_PULLUP);
}

void SensorManager::setupSensors(int sda_pin, int scl_pin) {
    Wire.begin(sda_pin, scl_pin);
    if (!bmp.begin()) {
        while (1) {}
    }
}

void SensorManager::setupMoistureSensors(const std::array<int, 4>& pins) {
    moistureSensorPins = pins;
    for (int pin : moistureSensorPins) {
        pinMode(pin, INPUT);
    }
}

void SensorManager::updateSensorData() {
    std::lock_guard<std::mutex> lock(dataMutex);

    for (int i = 0; i < 4; i++) {
        data.moisture[i] = readMoistureSensor(moistureSensorPins[i]);
    }
    data.temperature = bmp.readTemperature() + configManager.getTemperatureOffset();
    data.pressure = bmp.readPressure() / 100.0F;
    data.waterLevel = checkWaterLevel();
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
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    float average = sum / SAMPLES;
    return map(average, 0, 4095, 0, 100);
}

bool SensorManager::checkWaterLevel() {
    digitalWrite(floatSwitchPin, HIGH);
    vTaskDelay(pdMS_TO_TICKS(10));
    bool waterLevel = digitalRead(floatSwitchPin);
    digitalWrite(floatSwitchPin, LOW);
    return waterLevel;
}