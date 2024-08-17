#ifndef ESP_TELEMETRY_H
#define ESP_TELEMETRY_H

#include <ArduinoJson.h>
#include "ESPLogger.h"
#include "MQTTManager.h"  

class ESPTelemetry {
public:
    ESPTelemetry(ESPMQTTManager& mqttManager, 
                 const char* topic = "esp/telemetry")
        : logger(Logger::instance()),
          mqttManager(mqttManager), 
          topic(topic) {}

    void publishTelemetry() {
        logger.log(Logger::Level::INFO, "Preparing telemetry...");
        
        JsonDocument doc;
        
        // Heap info
        doc["heap"] = ESP.getFreeHeap();
        
        // WiFi info
        doc["wifi_rssi"] = WiFi.RSSI();
        doc["wifi_ssid"] = WiFi.SSID();
        
        // Uptime
        doc["uptime"] = millis() / 1000;
                    
        String telemetryJson;
        serializeJson(doc, telemetryJson);
        
        mqttManager.publish(topic, telemetryJson.c_str());
    }

    void setTopic(const char* newTopic) {
        topic = newTopic;
        logger.log(Logger::Level::INFO, "Telemetry topic set to: {}", newTopic);
    }

private:
    Logger& logger;
    ESPMQTTManager& mqttManager;
    const char* topic;
};

#endif // ESP_TELEMETRY_H