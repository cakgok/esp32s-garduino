#include "ESPTelemetry.h"
#include <WiFi.h>

ESPTelemetry::ESPTelemetry(ESPMQTTManager& mqttManager, const char* topic)
    : logger(Logger::instance()),
      mqttManager(mqttManager),
      topic(topic) {}

void ESPTelemetry::setTopic(const char* newTopic) {
    topic = newTopic;
    logger.log("Telemetry", Logger::Level::INFO, "Telemetry topic set to: {}", newTopic);
}

bool ESPTelemetry::publishTelemetry() {
    logger.log("Telemetry", Logger::Level::INFO, "Preparing telemetry...");
    
    JsonDocument doc;        
    doc["free_heap"] = xPortGetFreeHeapSize();

    if (WiFi.status() == WL_CONNECTED) {
        doc["wifi_rssi"] = WiFi.RSSI();
    }
    
    doc["uptime"] = millis() / 1000;
    doc["cpu_temp"] = temperatureRead();
    doc["max_free_heap_block"] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

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

void ESPTelemetry::addTaskToMonitor(TaskHandle_t task, const char* taskName) {
    monitoredTasks.push_back({task, taskName});
}