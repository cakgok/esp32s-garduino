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
            
            for (int i = 0; i < 4; i++) {
                doc["moisture" + String(i)] = data.moisture[i];
            }
            doc["temperature"] = data.temperature;
            doc["pressure"] = data.pressure;
            doc["waterLevel"] = data.waterLevel;

            String payload;
            serializeJson(doc, payload);
            
            mqttManager.publish("esp32/sensor_data", payload.c_str());

            // Delay for the publish interval
            uint32_t interval = configManager.getSensorPublishInterval();
            vTaskDelay(pdMS_TO_TICKS(interval));
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