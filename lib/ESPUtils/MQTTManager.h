#ifndef ESP_MQTT_MANAGER_H
#define ESP_MQTT_MANAGER_H

#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "ESPLogger.h"
#include <vector>
#include <queue>
#include <utility>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

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
        uint32_t reconnectInterval = 5000;
        uint32_t publishTimeout = 1000;
        uint16_t maxRetries = 5;
        AuthMode authMode = AuthMode::TLS_USER_PASS_AUTH;
        size_t publishBufferSize = 5;  // Can store up to 20 messages
    };

    struct PublishItem {
        String topic;
        String payload;
        bool retained;
    };

    ESPMQTTManager(const Config& config);
    ~ESPMQTTManager();

    bool begin();
    void stop();
    bool publish(const char* topic, const char* payload, bool retained = false);
    bool subscribe(const char* topic, uint8_t qos = 0);
    bool isConnected();
    void setCallback(MQTT_CALLBACK_SIGNATURE);
    void setAuthMode(AuthMode mode);
    PubSubClient& getClient();

private:
    static void taskWrapper(void* pvParameters);
    void task();
    bool connect();
    void disconnect();
    void resubscribe();
    void setupTLS();
    String getClientId() const;
    void processPublishBuffer();

    Logger& logger;
    Config config;
    WiFiClientSecure espClient;
    PubSubClient mqttClient;
    TaskHandle_t taskHandle;
    SemaphoreHandle_t mqttMutex;
    std::vector<std::pair<String, uint8_t>> subscriptions;
    volatile bool running;
    uint16_t retryCount;
    QueueHandle_t publishBuffer;
};

#endif // ESP_MQTT_MANAGER_H