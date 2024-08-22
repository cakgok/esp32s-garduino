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
    static constexpr uint32_t STARTUP_DELAY_MS = 10000; // 10 second delay

    static void taskFunction(void* pvParameters) {
        SensorPublishTask* self = static_cast<SensorPublishTask*>(pvParameters);
        self->delayedStart();
    }

    void delayedStart() {
        Logger::instance().log("PublishManager", Logger::Level::INFO, "Waiting before starting sensor publish task...");
        vTaskDelay(pdMS_TO_TICKS(STARTUP_DELAY_MS));
        Logger::instance().log("PublishManager", Logger::Level::INFO, "Starting sensor publish task");
        publishSensorData();
    }

    void publishSensorData() {
        while (true) {
            if (mqttManager.isConnected()) {
                SensorData data = sensorManager.getSensorData();
                JsonDocument doc;
                
                for (size_t i = 0; i < ConfigConstants::RELAY_COUNT; ++i) {
                    const auto& config = configManager.getCachedSensorConfig(i);
                    if (config.sensorEnabled) {
                        doc["moisture_" + String(config.sensorPin)] = data.moisture[i];
                    }
                }
                doc["temperature"] = data.temperature;
                doc["pressure"] = data.pressure;
                doc["waterLevel"] = data.waterLevel;

                String payload;
                serializeJson(doc, payload);
                
                mqttManager.publish("esp32/sensor_data", payload.c_str());

                const auto& softwareConfig = configManager.getCachedSoftwareConfig();
                uint32_t publishInterval = softwareConfig.sensorPublishInterval;
                vTaskDelay(pdMS_TO_TICKS(publishInterval));
            } else {
                Logger::instance().log("PublishManager", Logger::Level::WARNING, "MQTT not connected. Waiting before retry.");
                vTaskDelay(pdMS_TO_TICKS(5000)); // Wait 5 seconds before checking connection again
            }
        }
    }

public:
    SensorPublishTask(SensorManager& sm, ESPMQTTManager& mm, ConfigManager& cm)
        : sensorManager(sm), mqttManager(mm), configManager(cm), taskHandle(NULL) {}

    void start() {
        xTaskCreate(
            taskFunction,
            "SensorPublishTask",
            8192,  // Stack size (adjust as needed)
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