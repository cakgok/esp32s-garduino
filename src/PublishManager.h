// PublishManager.h

#ifndef PUBLISH_MANAGER_H
#define PUBLISH_MANAGER_H

#include <ArduinoJson.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "SensorManager.h"
#include "ConfigManager.h"
#include "MQTTManager.h"
#include "ESPLogger.h"
#include "ESPTelemetry.h"

class PublishManager {
private:
    SensorManager& sensorManager;
    ESPMQTTManager& mqttManager;
    ConfigManager& configManager;
    ESPTelemetry telemetry;
    TaskHandle_t sensorTaskHandle;
    TaskHandle_t telemetryTaskHandle;
    static constexpr uint32_t STARTUP_DELAY_MS = 30000; // 30 second delay
    Logger& logger;

    static void sensorTaskFunction(void* pvParameters) {
        PublishManager* self = static_cast<PublishManager*>(pvParameters);
        self->delayedStart(true);
    }

    static void telemetryTaskFunction(void* pvParameters) {
        PublishManager* self = static_cast<PublishManager*>(pvParameters);
        self->delayedStart(false);
    }

    void delayedStart(bool isSensorTask) {
        logger.log("PublishManager", Logger::Level::INFO, "Waiting before starting {} publish task...", isSensorTask ? "sensor" : "telemetry");
        vTaskDelay(pdMS_TO_TICKS(STARTUP_DELAY_MS));
        logger.log("PublishManager", Logger::Level::INFO, "Starting {} publish task", isSensorTask ? "sensor" : "telemetry");
        if (isSensorTask) {
            publishSensorData();
        } else {
            publishTelemetryData();
        }
    }

    void publishSensorData() {
        while (true) {
            SensorData data = sensorManager.getSensorData();
            JsonDocument doc;
            
            const auto& hwConf = configManager.getHwConfig();
            for (size_t i = 0; i < hwConf.systemSize.value(); i++) {
                const auto& config = configManager.getSensorConfig(i);
                if (config.sensorEnabled) {
                    doc["moisture_" + String(i)] = data.moisture[i];
                }
            }
            doc["temperature"] = data.temperature;
            doc["pressure"] = data.pressure;
            doc["waterLevel"] = data.waterLevel;

            String payload;
            serializeJson(doc, payload);
            
            if (mqttManager.publish("esp32/sensor_data", payload.c_str())) {
                logger.log("PublishManager", Logger::Level::INFO, "Published sensor data successfully");
            } else {
                logger.log("PublishManager", Logger::Level::ERROR, "Failed to publish sensor data");
            }

            const auto& softwareConfig = configManager.getSwConfig();
            uint32_t publishInterval = softwareConfig.sensorPublishInterval.value();
            vTaskDelay(pdMS_TO_TICKS(publishInterval));
        }
    }

    void publishTelemetryData() {
        while (true) {
            if (telemetry.publishTelemetry()) {
                logger.log("PublishManager", Logger::Level::INFO, "Published telemetry data successfully");
            } else {
                logger.log("PublishManager", Logger::Level::ERROR, "Failed to publish telemetry data");
            }

            const auto& softwareConfig = configManager.getSwConfig();
            uint32_t telemetryInterval = softwareConfig.telemetryInterval.value();
            vTaskDelay(pdMS_TO_TICKS(telemetryInterval));
        }
    }

public:
    PublishManager(SensorManager& sm, ESPMQTTManager& mm, ConfigManager& cm)
        : sensorManager(sm), mqttManager(mm), configManager(cm), 
          telemetry(mm, "esp32/telemetry"), 
          sensorTaskHandle(NULL), telemetryTaskHandle(NULL), 
          logger(Logger::instance()) {}

    void start() {
        xTaskCreate(
            sensorTaskFunction,
            "SensorPublish",
            8192,  // Stack size
            this,
            1,  // Priority
            &sensorTaskHandle
        );

        xTaskCreate(
            telemetryTaskFunction,
            "TelemetryPublish",
            8192,  // Stack size
            this,
            1,  // Priority
            &telemetryTaskHandle
        );

        vTaskDelay(pdMS_TO_TICKS(STARTUP_DELAY_MS + 1000));
        // Setup telemetry data after tasks have started
        setupTelemetryData();
    }

    ~PublishManager() {
        if (sensorTaskHandle != NULL) {
            vTaskDelete(sensorTaskHandle);
        }
        if (telemetryTaskHandle != NULL) {
            vTaskDelete(telemetryTaskHandle);
        }
    }

    ESPTelemetry& getTelemetry() {
        return telemetry;
    }

    // New getter methods for task handles
    TaskHandle_t getSensorTaskHandle() const {
        return sensorTaskHandle;
    }

    TaskHandle_t getTelemetryTaskHandle() const {
        return telemetryTaskHandle;
    }

    void setupTelemetryData() {
        telemetry.addCustomData("publishManager_telemetry_stack_hwm", [this]() -> UBaseType_t {
            return uxTaskGetStackHighWaterMark(telemetryTaskHandle);
        });

        telemetry.addCustomData("publishManager_sensor_stack_hwm", [this]() -> UBaseType_t {
            return uxTaskGetStackHighWaterMark(sensorTaskHandle);
        });

        telemetry.addCustomData("sensor_task_stack_hwm", [this]() -> UBaseType_t {
            return uxTaskGetStackHighWaterMark(sensorManager.getTaskHandle());
        });

        logger.log("PublishManager", Logger::Level::INFO, "Telemetry custom data setup complete");
    }

};

#endif // PUBLISH_MANAGER_H