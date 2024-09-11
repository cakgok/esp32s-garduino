#ifndef ESP_MQTT_MANAGER_H
#define ESP_MQTT_MANAGER_H

#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "ESPLogger.h"
#include <vector>
#include <utility>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class ESPMQTTManager {
public:
    enum class AuthMode { TLS_CERT_AUTH, TLS_USER_PASS_AUTH };

    struct Config {
        const char* server;
        int port;
        const char* username;
        const char* password;
        const char* rootCA;
        const char* clientCert;
        const char* clientKey;
        const char* clientID;
        uint32_t reconnectInterval;
        uint32_t publishTimeout;
        uint16_t maxRetries;
        AuthMode authMode;
    };

    ESPMQTTManager(const Config& config);
    ~ESPMQTTManager();

    bool begin();
    void startLoopTask();
    void stopLoopTask();
    bool publish(const char* topic, const char* payload, bool retained = false);
    bool subscribe(const char* topic, uint8_t qos = 0);
    bool isConnected();
    void setCallback(MQTT_CALLBACK_SIGNATURE);
    void setAuthMode(AuthMode mode);
    PubSubClient& getClient();

    void setReconnectInterval(uint32_t interval) { reconnectInterval = interval; }
    void setPublishTimeout(uint32_t timeout) { publishTimeout = timeout; }
    void setMaxRetries(uint16_t max) { maxRetries = max; }
    uint16_t getRetryCount() const { return retryCount; }

private:
    static void reconnectTask(void* pvParameters);
    static void loopTask(void* pvParameters);
    bool connect();
    void disconnect();
    void maintainConnection();    
    bool attemptConnection();
    void resubscribe();
    void setupTLS();
    String getClientId() const;

    Logger& logger;
    Config config;
    WiFiClientSecure espClient;
    PubSubClient mqttClient;
    TaskHandle_t reconnectTaskHandle;
    TaskHandle_t loopTaskHandle;
    uint32_t reconnectInterval;
    uint32_t publishTimeout;
    uint32_t lastReconnectAttempt;
    uint16_t maxRetries;
    uint16_t retryCount;
    std::vector<std::pair<String, uint8_t>> subscriptions;
    SemaphoreHandle_t mqttMutex;
};

// Implementation

ESPMQTTManager::ESPMQTTManager(const Config& config)
    : logger(Logger::instance()),
      config(config),
      espClient(),
      mqttClient(espClient),
      reconnectTaskHandle(NULL),
      loopTaskHandle(NULL),
      reconnectInterval(config.reconnectInterval ? config.reconnectInterval : 5000),
      publishTimeout(config.publishTimeout ? config.publishTimeout : 5000),
      maxRetries(config.maxRetries ? config.maxRetries : 5),
      retryCount(0),
      mqttMutex(xSemaphoreCreateMutex()) {}

ESPMQTTManager::~ESPMQTTManager() {
    if (reconnectTaskHandle != NULL) {
        vTaskDelete(reconnectTaskHandle);
    }
    if (loopTaskHandle != NULL) {
        vTaskDelete(loopTaskHandle);
    }
    vSemaphoreDelete(mqttMutex);
}

bool ESPMQTTManager::begin() {
    setupTLS();
    mqttClient.setServer(config.server, config.port);
    mqttClient.setKeepAlive(60); // Set keep-alive to 60 seconds
    logger.log("MQTTManager", Logger::Level::INFO, "MQTT client configured with TLS");

    if (connect()) {
        startLoopTask();
        return true;
    }
    return false;
}

bool ESPMQTTManager::connect() {
    String clientId = getClientId();
    logger.log("MQTTManager", Logger::Level::INFO, "Attempting connection with client ID: %s", clientId.c_str());

    bool connected = false;
    if (config.authMode == AuthMode::TLS_CERT_AUTH) {
        connected = mqttClient.connect(clientId.c_str());
    } else {
        connected = mqttClient.connect(clientId.c_str(), config.username, config.password);
    }

    if (connected) {
        logger.log("MQTTManager", Logger::Level::INFO, "Connected to MQTT broker");
        resubscribe();
    } else {
        logger.log("MQTTManager", Logger::Level::ERROR, "Failed to connect, rc=%d", mqttClient.state());
    }

    return connected;
}

void ESPMQTTManager::disconnect() {
    mqttClient.disconnect();
    logger.log("MQTTManager", Logger::Level::INFO, "Disconnected from MQTT broker");
}

void ESPMQTTManager::maintainConnection() {
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > reconnectInterval) {
            lastReconnectAttempt = now;
            logger.log("MQTTManager", Logger::Level::INFO, "Connection lost. Attempting reconnection...");
            if (connect()) {
                lastReconnectAttempt = 0;
            }
        }
    }
}

void ESPMQTTManager::startLoopTask() {
    if (loopTaskHandle == NULL) {
        xTaskCreate(
            loopTask,
            "MQTT Loop",
            4096,
            this,
            1,
            &loopTaskHandle
        );
        logger.log("MQTTManager", Logger::Level::INFO, "MQTT loop task started");
    } else {
        logger.log("MQTTManager", Logger::Level::WARNING, "MQTT loop task already running");
    }
}

void ESPMQTTManager::stopLoopTask() {
    if (loopTaskHandle != NULL) {
        vTaskDelete(loopTaskHandle);
        loopTaskHandle = NULL;
        logger.log("MQTTManager", Logger::Level::INFO, "MQTT loop task stopped");
    }
}

void ESPMQTTManager::setCallback(MQTT_CALLBACK_SIGNATURE) {
    mqttClient.setCallback(callback);
    logger.log("MQTTManager", Logger::Level::INFO, "MQTT callback set");
}

void ESPMQTTManager::reconnectTask(void* pvParameters) {
    ESPMQTTManager* manager = static_cast<ESPMQTTManager*>(pvParameters);
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(manager->reconnectInterval);

    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        if (xSemaphoreTake(manager->mqttMutex, portMAX_DELAY) == pdTRUE) {
            if (!manager->mqttClient.connected()) {
                manager->logger.log("MQTTManager", Logger::Level::INFO, "Attempting MQTT connection... (Attempt %d of %d)", manager->retryCount + 1, manager->maxRetries);

                if (manager->attemptConnection()) {
                    manager->logger.log("MQTTManager", Logger::Level::INFO, "Connected to MQTT broker");
                    manager->retryCount = 0;
                    manager->resubscribe();
                } else {
                    manager->retryCount++;
                    manager->logger.log("MQTTManager", Logger::Level::ERROR, "Failed to connect to MQTT broker, rc=%d, retry=%d/%d", manager->mqttClient.state(), manager->retryCount, manager->maxRetries);

                    if (manager->retryCount >= manager->maxRetries) {
                        manager->logger.log("MQTTManager", Logger::Level::ERROR, "Max retries reached. Resetting retry count.");
                        manager->retryCount = 0;
                    }
                }
            }
            xSemaphoreGive(manager->mqttMutex);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void ESPMQTTManager::loopTask(void* pvParameters) {
    ESPMQTTManager* manager = static_cast<ESPMQTTManager*>(pvParameters);
    
    for (;;) {
        if (xSemaphoreTake(manager->mqttMutex, portMAX_DELAY) == pdTRUE) {
            manager->mqttClient.loop();
            xSemaphoreGive(manager->mqttMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent task from hogging CPU
    }
}

void ESPMQTTManager::setAuthMode(AuthMode mode) {
    if (config.authMode != mode) {
        config.authMode = mode;
        logger.log("MQTTManager", Logger::Level::INFO, "Switched authentication mode. Reconnection required.");
    }
}

bool ESPMQTTManager::publish(const char* topic, const char* payload, bool retained) {
    if (xSemaphoreTake(mqttMutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    bool result = false;
    if (mqttClient.connected()) {
        result = mqttClient.publish(topic, payload, retained);
        if (result) {
            logger.log("MQTTManager", Logger::Level::INFO, "Published to topic: %s", topic);
        } else {
            logger.log("MQTTManager", Logger::Level::ERROR, "Failed to publish to topic: %s", topic);
        }
    } else {
        logger.log("MQTTManager", Logger::Level::ERROR, "Not connected to MQTT broker");
    }

    xSemaphoreGive(mqttMutex);
    return result;
}

bool ESPMQTTManager::subscribe(const char* topic, uint8_t qos) {
    if (xSemaphoreTake(mqttMutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    bool result = false;
    if (mqttClient.connected()) {
        if (mqttClient.subscribe(topic, qos)) {
            subscriptions.push_back(std::make_pair(String(topic), qos));
            logger.log("MQTTManager", Logger::Level::INFO, "Subscribed to topic: %s", topic);
            result = true;
        } else {
            logger.log("MQTTManager", Logger::Level::ERROR, "Failed to subscribe to topic: %s", topic);
        }
    } else {
        logger.log("MQTTManager", Logger::Level::ERROR, "Not connected to MQTT broker");
    }

    xSemaphoreGive(mqttMutex);
    return result;
}

bool ESPMQTTManager::isConnected() {
    if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
        bool connected = mqttClient.connected();
        xSemaphoreGive(mqttMutex);
        return connected;
    }
    return false;
}

PubSubClient& ESPMQTTManager::getClient() {
    return mqttClient;
}

// Private methods

bool ESPMQTTManager::attemptConnection() {
    String clientId = getClientId();
    logger.log("MQTTManager", Logger::Level::INFO, "Using client ID: %s", clientId.c_str());

    if (config.authMode == AuthMode::TLS_CERT_AUTH) {
        logger.log("MQTTManager", Logger::Level::INFO, "Attempting connection with TLS certificate authentication");
        return mqttClient.connect(clientId.c_str());
    } else {
        logger.log("MQTTManager", Logger::Level::INFO, "Attempting connection with TLS and username/password authentication");
        return mqttClient.connect(clientId.c_str(), config.username, config.password);
    }
}

void ESPMQTTManager::resubscribe() {
    for (const auto& sub : subscriptions) {
        if (mqttClient.subscribe(sub.first.c_str(), sub.second)) {
            logger.log("MQTTManager", Logger::Level::INFO, "Resubscribed to topic: %s", sub.first.c_str());
        } else {
            logger.log("MQTTManager", Logger::Level::ERROR, "Failed to resubscribe to topic: %s", sub.first.c_str());
        }
    }
}

void ESPMQTTManager::setupTLS() {
    espClient.setCACert(config.rootCA);
    espClient.setCertificate(config.clientCert);
    espClient.setPrivateKey(config.clientKey);
}

String ESPMQTTManager::getClientId() const {
    return (config.clientID && strcmp(config.clientID, "random") != 0) 
        ? config.clientID 
        : "ESPClient-" + String(random(0xffff), HEX);
}

#endif // ESP_MQTT_MANAGER_H