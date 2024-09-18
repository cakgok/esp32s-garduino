#ifndef ESP_TELEMETRY_H
#define ESP_TELEMETRY_H

#include <ArduinoJson.h>
#include <functional>
#include <map>
#include <vector>
#include "ESPLogger.h"
#include "MQTTManager.h"

class ESPTelemetry {
public:
    ESPTelemetry(ESPMQTTManager& mqttManager, const char* topic = "esp/telemetry");

    void setTopic(const char* newTopic);

    template<typename T>
    void addCustomData(const char* key, std::function<T()> dataProvider);

    void addCustomData(const char* key, std::function<UBaseType_t()> dataProvider);

    bool publishTelemetry();

    void addTaskToMonitor(TaskHandle_t task, const char* taskName);

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

template<typename T>
void ESPTelemetry::addCustomData(const char* key, std::function<T()> dataProvider) {
    customData[key] = [dataProvider]() -> String {
        return String(dataProvider());
    };
}

// Specialization for UBaseType_t
inline void ESPTelemetry::addCustomData(const char* key, std::function<UBaseType_t()> dataProvider) {
    customData[key] = [dataProvider]() -> String {
        return String(static_cast<unsigned long>(dataProvider()));
    };
}


#endif // ESP_TELEMETRY_H