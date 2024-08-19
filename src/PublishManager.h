// PublishManager.h

#ifndef PUBLISH_MANAGER_H
#define PUBLISH_MANAGER_H

#include <ArduinoJson.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "SensorManager.h"
#include "ConfigManager.h"
#include "MQTTManager.h"

class SensorPublishTask {
private:
    SensorManager& sensorManager;
    ESPMQTTManager& mqttManager;
    ConfigManager& configManager;
    TaskHandle_t taskHandle;

    static void taskFunction(void* pvParameters) {
        SensorPublishTask* self = static_cast<SensorPublishTask*>(pvParameters);
        self->publishSensorData();
    }

    void publishSensorData() {
        while (true) {
            SensorData data = sensorManager.getSensorData();
            JsonDocument doc;
            
            auto configs = configManager.getEnabledSensorConfigs();
            for (const auto& config : configs) {
                if (config.sensorEnabled) {
                    doc["moisture_" + String(config.sensorPin)] = data.moisture[config.sensorPin];
                }
            }
            doc["temperature"] = data.temperature;
            doc["pressure"] = data.pressure;
            doc["waterLevel"] = data.waterLevel;

            String payload;
            serializeJson(doc, payload);
            
            mqttManager.publish("esp32/sensor_data", payload.c_str());

            // Delay for the publish interval
            ConfigManager::ConfigValue interval = configManager.getValue(ConfigManager::ConfigKey::SENSOR_PUBLISH_INTERVAL);
            // Directly extract the uint32_t value using std::get
            uint32_t publishInterval = std::get<uint32_t>(interval);
            vTaskDelay(pdMS_TO_TICKS(publishInterval));
        }
    }

public:
    SensorPublishTask(SensorManager& sm, ESPMQTTManager& mm, ConfigManager& cm)
        : sensorManager(sm), mqttManager(mm), configManager(cm), taskHandle(NULL) {}

    void start() {
        xTaskCreate(
            taskFunction,
            "SensorPublishTask",
            4096,  // Stack size (adjust as needed)
            this,
            1,  // Priority
            &taskHandle
        );
    }

    ~SensorPublishTask() {
        if (taskHandle != NULL) {
            vTaskDelete(taskHandle);
        }
    }
};

#endif // PUBLISH_MANAGER_H