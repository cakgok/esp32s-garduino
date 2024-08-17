#ifndef ESP_MQTT_MANAGER_H
#define ESP_MQTT_MANAGER_H

#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "ESPLogger.h"  // Make sure this is the correct path to your new Logger class

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
        const char* clientID;
    };

    ESPMQTTManager(const Config& config)
        : logger(Logger::instance()),  // Get the logger instance
          config(config),
          espClient(),
          mqttClient(espClient) {}

    void setup() {
        espClient.setCACert(config.rootCA);
        espClient.setCertificate(config.clientCert);
        espClient.setPrivateKey(config.clientKey);
        mqttClient.setServer(config.server, config.port);
        logger.log(Logger::Level::INFO, "MQTT client configured");
    }

    void setCallback(MQTT_CALLBACK_SIGNATURE) {
        mqttClient.setCallback(callback);
        logger.log(Logger::Level::INFO, "MQTT callback set");
    }

    bool reconnect() {
        if (mqttClient.connected()) {
            return true;
        }
        logger.log(Logger::Level::INFO, "Attempting MQTT connection...");
        
        String clientId;
        if (config.clientID && strcmp(config.clientID, "random") != 0) {
            clientId = config.clientID;
        } else {
            clientId = "ESPClient-" + String(random(0xffff), HEX);
        }
        
        logger.log(Logger::Level::INFO, "Using client ID: {}", clientId.c_str());
        
        if (mqttClient.connect(clientId.c_str(), config.username, config.password)) {
            logger.log(Logger::Level::INFO, "Connected to MQTT broker");
            return true;
        } else {
            logger.log(Logger::Level::ERROR, "Failed to connect to MQTT broker, rc={}", mqttClient.state());
            return false;
        }
    }

    bool publish(const char* topic, const char* payload) {
        if (!mqttClient.connected() && !reconnect()) {
            logger.log(Logger::Level::ERROR, "Failed to publish: not connected");
            return false;
        }
        bool result = mqttClient.publish(topic, payload);
        if (result) {
            logger.log(Logger::Level::INFO, "Published to topic: {}", topic);
        } else {
            logger.log(Logger::Level::ERROR, "Failed to publish to topic: {}", topic);
        }
        return result;
    }

    bool subscribe(const char* topic) {
        if (!mqttClient.connected() && !reconnect()) {
            logger.log(Logger::Level::ERROR, "Failed to subscribe: not connected");
            return false;
        }
        bool result = mqttClient.subscribe(topic);
        if (result) {
            logger.log(Logger::Level::INFO, "Subscribed to topic: {}", topic);
        } else {
            logger.log(Logger::Level::ERROR, "Failed to subscribe to topic: {}", topic);
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
    Logger& logger;
    Config config;
    WiFiClientSecure espClient;
    PubSubClient mqttClient;
};

#endif // ESP_MQTT_MANAGER_H