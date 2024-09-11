#ifndef ESP_TELEMETRY_H
#define ESP_TELEMETRY_H

#include <ArduinoJson.h>
#include <functional>
#include <map>
#include "ESPLogger.h"
#include "MQTTManager.h"

class ESPTelemetry {
public:
    ESPTelemetry(ESPMQTTManager& mqttManager, const char* topic = "esp/telemetry")
        : logger(Logger::instance()),
          mqttManager(mqttManager),
          topic(topic) {}

    void setTopic(const char* newTopic) {
        topic = newTopic;
        logger.log("Telemetry", Logger::Level::INFO, "Telemetry topic set to: {}", newTopic);
    }

   template<typename T>
    void addCustomData(const char* key, std::function<T()> dataProvider) {
        customData[key] = [dataProvider]() -> String {
            return String(dataProvider());
        };
    }

    // Specialization for UBaseType_t
    void addCustomData(const char* key, std::function<UBaseType_t()> dataProvider) {
        customData[key] = [dataProvider]() -> String {
            return String(static_cast<unsigned long>(dataProvider()));
        };
    }

    bool publishTelemetry() {
        logger.log("Telemetry", Logger::Level::INFO, "Preparing telemetry...");
        
        JsonDocument doc;        
        // Standard telemetry data
        doc["heap"] = xPortGetFreeHeapSize();

        if (WiFi.status() == WL_CONNECTED) {
            doc["wifi_rssi"] = WiFi.RSSI();
            doc["wifi_ssid"] = WiFi.SSID();
        }
        
        doc["uptime"] = millis() / 1000;
        doc["cpuTemp"] = temperatureRead();

        // Add custom data
        for (const auto& [key, dataProvider] : customData) {
            doc[key] = dataProvider();
        }

        String telemetryJson;
        serializeJson(doc, telemetryJson);
        
        if (mqttManager.publish(topic, telemetryJson.c_str())) {
            logger.log("Telemetry", Logger::Level::INFO, "Telemetry published successfully");
            return true;
        } else {
            logger.log("Telemetry", Logger::Level::ERROR, "Failed to publish telemetry");
            return false;
        }
    }

    void addTaskToMonitor(TaskHandle_t task, const char* taskName) {
        monitoredTasks.push_back({task, taskName});
    }


private:
    Logger& logger;
    ESPMQTTManager& mqttManager;
    const char* topic;
    std::map<const char*, std::function<String()>> customData;

    struct MonitoredTask {
        TaskHandle_t handle;
        const char* name;
    };
    std::vector<MonitoredTask> monitoredTasks;

};

#endif // ESP_TELEMETRY_H