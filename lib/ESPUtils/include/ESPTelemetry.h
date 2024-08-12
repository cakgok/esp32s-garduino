#ifndef ESP_TELEMETRY_H
#define ESP_TELEMETRY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include "ESPLogger.h"

template<size_t BUFFER_SIZE = 50>
class ESPTelemetry {
public:
    ESPTelemetry(PubSubClient& mqttClient, 
                 ESPLogger<BUFFER_SIZE>& logger, 
                 unsigned long interval = 60000, 
                 const char* topic = "esp8266/telemetry")
        : mqttClient(mqttClient), 
          logger(logger), 
          interval(interval), 
          topic(topic), 
          lastMonitorTime(0) {}

    void monitorAndPublish() {
        unsigned long currentTime = millis();
        if (currentTime - lastMonitorTime >= interval) {
            lastMonitorTime = currentTime;
            publishTelemetry();
        }
    }

    void setInterval(unsigned long newInterval) {
        interval = newInterval;
    }

    void setTopic(const char* newTopic) {
        topic = newTopic;
    }

private:
    void publishTelemetry() {
        if (mqttClient.connected()) {
            logger.log("MQTT client is connected. Preparing telemetry...");
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
            
            if (mqttClient.publish(topic, telemetryJson.c_str())) {
                logger.log("Successfully published telemetry: %s", telemetryJson.c_str());
            } else {
                logger.log("Failed to publish telemetry. MQTT client state: %d", mqttClient.state());
            }
        } else {
            logger.log("MQTT client is not connected. Cannot publish telemetry.");
        }
    }

    PubSubClient& mqttClient;
    ESPLogger<BUFFER_SIZE>& logger;
    unsigned long interval;
    const char* topic;
    unsigned long lastMonitorTime;
};

#endif // ESP_TELEMETRY_H