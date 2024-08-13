#ifndef ESP_MQTT_MANAGER_H
#define ESP_MQTT_MANAGER_H

#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "ESPLogger.h"

class ESPMQTTManager {
public:
    struct Config {
        const char* server;
        int port;
        const char* username;
        const char* password;
        const char* rootCA;
        const char* clientCert;
        const char* clientKey;
        const char* clientID;  // New field for client ID
    };

    ESPMQTTManager(ESPLogger* logger, const Config& config)
        : logger(logger), 
          config(config),
          espClient(),
          mqttClient(espClient) {}

    void setup() {
        espClient.setCACert(config.rootCA);
        espClient.setCertificate(config.clientCert);
        espClient.setPrivateKey(config.clientKey);
        mqttClient.setServer(config.server, config.port);
        if (logger) logger->log(LogLevel::INFO, "MQTT client configured");
    }

    void setCallback(MQTT_CALLBACK_SIGNATURE) {
        mqttClient.setCallback(callback);
        if (logger) logger->log(LogLevel::INFO, "MQTT callback set");
    }

    bool reconnect() {
        if (mqttClient.connected()) {
            return true;
        }
        if (logger) logger->log(LogLevel::INFO, "Attempting MQTT connection...");
        
        String clientId;
        if (config.clientID && strcmp(config.clientID, "random") != 0) {
            clientId = config.clientID;
        } else {
            clientId = "ESPClient-" + String(random(0xffff), HEX);
        }
        
        if (logger) logger->log(LogLevel::INFO, "Using client ID: %s", clientId.c_str());
        
        if (mqttClient.connect(clientId.c_str(), config.username, config.password)) {
            if (logger) logger->log(LogLevel::INFO, "Connected to MQTT broker");
            return true;
        } else {
            if (logger) logger->log(LogLevel::ERROR, "Failed to connect to MQTT broker, rc=%d", mqttClient.state());
            return false;
        }
    }

    bool publish(const char* topic, const char* payload) {
        if (!mqttClient.connected() && !reconnect()) {
            if (logger) logger->log(LogLevel::ERROR, "Failed to publish: not connected");
            return false;
        }
        bool result = mqttClient.publish(topic, payload);
        if (logger) {
            if (result) {
                logger->log(LogLevel::INFO, "Published to topic: %s", topic);
            } else {
                logger->log(LogLevel::ERROR, "Failed to publish to topic: %s", topic);
            }
        }
        return result;
    }

    bool subscribe(const char* topic) {
        if (!mqttClient.connected() && !reconnect()) {
            if (logger) logger->log(LogLevel::ERROR, "Failed to subscribe: not connected");
            return false;
        }
        bool result = mqttClient.subscribe(topic);
        if (logger) {
            if (result) {
                logger->log(LogLevel::INFO, "Subscribed to topic: %s", topic);
            } else {
                logger->log(LogLevel::ERROR, "Failed to subscribe to topic: %s", topic);
            }
        }
        return result;
    }

    void loop() {
        if (!mqttClient.connected()) {
            reconnect();
        }
        mqttClient.loop();
    }

    PubSubClient& getClient() {
        return mqttClient;
    }

private:
    ESPLogger* logger;
    Config config;
    WiFiClientSecure espClient;
    PubSubClient mqttClient;
};

#endif // ESP_MQTT_MANAGER_H