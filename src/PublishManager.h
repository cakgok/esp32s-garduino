#ifndef PUBLISH_MANAGER_H
#define PUBLISH_MANAGER_H

#include <ArduinoJson.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "SensorManager.h"
#include "ConfigManager.h"
#include "MQTTManager.h"

class SensorPublishTask {
private:
    SensorManager& sensorManager;
    ESPMQTTManager& mqttManager;
    ConfigManager& configManager;
    TimerHandle_t timer;

    static void timerCallback(TimerHandle_t xTimer) {
        SensorPublishTask* self = static_cast<SensorPublishTask*>(pvTimerGetTimerID(xTimer));
        self->publishSensorData();
    }

    void publishSensorData() {
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

        // Update the timer interval for the next publish
        uint32_t newInterval = configManager.getSensorPublishInterval();
        xTimerChangePeriod(timer, pdMS_TO_TICKS(newInterval), 0);
    }

public:
    SensorPublishTask(SensorManager& sm, ESPMQTTManager& mm, ConfigManager& cm)
        : sensorManager(sm), mqttManager(mm), configManager(cm) {
        
        timer = xTimerCreate(
            "SensorPublishTimer",
            pdMS_TO_TICKS(configManager.getSensorPublishInterval()),
            pdFALSE,  // Auto-reload
            this,
            timerCallback
        );
    }

    void start() {
        xTimerStart(timer, 0);
    }

    void stop() {
        xTimerStop(timer, 0);
    }

    ~SensorPublishTask() {
        xTimerDelete(timer, 0);
    }
};

#endif // PUBLISH_MANAGER_H