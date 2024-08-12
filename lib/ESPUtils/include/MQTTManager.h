#ifndef ESP_MQTT_MANAGER_H
#define ESP_MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "ESPLogger.h"

template<size_t BUFFER_SIZE = 50>
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
    };

    ESPMQTTManager(ESPLogger<BUFFER_SIZE>& logger, const Config& config)
        : logger(logger), 
          config(config),
          espClient(),
          mqttClient(espClient) {}

    void setup() {
        espClient.setCACert(config.rootCA);
        espClient.setCertificate(config.clientCert);
        espClient.setPrivateKey(config.clientKey);

        mqttClient.setServer(config.server, config.port);
        logger.log("MQTT client configured");
    }

    void setCallback(MQTT_CALLBACK_SIGNATURE) {
        mqttClient.setCallback(callback);
    }

    bool reconnect() {
        if (mqttClient.connected()) {
            return true;
        }

        logger.log("Attempting MQTT connection...");
        String clientId = "ESPClient-" + String(random(0xffff), HEX);
        if (mqttClient.connect(clientId.c_str(), config.username, config.password)) {
            logger.log("Connected to MQTT broker");
            return true;
        } else {
            logger.log("Failed to connect to MQTT broker, rc=%d", mqttClient.state());
            return false;
        }
    }

    bool publish(const char* topic, const char* payload) {
        if (!mqttClient.connected() && !reconnect()) {
            return false;
        }
        return mqttClient.publish(topic, payload);
    }

    bool subscribe(const char* topic) {
        if (!mqttClient.connected() && !reconnect()) {
            return false;
        }
        return mqttClient.subscribe(topic);
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
    ESPLogger<BUFFER_SIZE>& logger;
    Config config;
    WiFiClientSecure espClient;
    PubSubClient mqttClient;
};

#endif // ESP_MQTT_MANAGER_H