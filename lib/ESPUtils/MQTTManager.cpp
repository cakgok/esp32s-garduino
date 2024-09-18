#include "MQTTManager.h"

ESPMQTTManager::ESPMQTTManager(const Config& config)
    : logger(Logger::instance()),
      config(config),
      espClient(),
      mqttClient(espClient),
      taskHandle(NULL),
      mqttMutex(xSemaphoreCreateMutex()),
      running(false),
      retryCount(0),
      publishBuffer(xQueueCreate(config.publishBufferSize, sizeof(PublishItem))) {}

ESPMQTTManager::~ESPMQTTManager() {
    stop();
    vSemaphoreDelete(mqttMutex);
    vQueueDelete(publishBuffer);
}

bool ESPMQTTManager::begin() {
    setupTLS();
    mqttClient.setServer(config.server, config.port);
    mqttClient.setKeepAlive(60);
    logger.log("MQTTManager", Logger::Level::INFO, "MQTT client configured with TLS");

    running = true;
    BaseType_t result = xTaskCreate(taskWrapper, "MQTT Task", 8192, this, 1, &taskHandle);
    if (result != pdPASS) {
        logger.log("MQTTManager", Logger::Level::ERROR, "Failed to create MQTT task");
        running = false;
        return false;
    }
    return true;
}

void ESPMQTTManager::stop() {
    running = false;
    if (taskHandle != NULL) {
        vTaskDelete(taskHandle);
        taskHandle = NULL;
    }
    disconnect();
}

void ESPMQTTManager::taskWrapper(void* pvParameters) {
    ESPMQTTManager* manager = static_cast<ESPMQTTManager*>(pvParameters);
    manager->task();
}

void ESPMQTTManager::task() {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(config.reconnectInterval);

    while (running) {
        if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
            if (!mqttClient.connected()) {
                logger.log("MQTTManager", Logger::Level::INFO, "Attempting MQTT connection... (Attempt %d of %d)", retryCount + 1, config.maxRetries);
                
                if (connect()) {
                    logger.log("MQTTManager", Logger::Level::INFO, "Connected to MQTT broker");
                    retryCount = 0;
                    resubscribe();
                } else {
                    retryCount++;
                    logger.log("MQTTManager", Logger::Level::ERROR, "Failed to connect to MQTT broker, rc=%d, retry=%d/%d", mqttClient.state(), retryCount, config.maxRetries);
                    
                    if (retryCount >= config.maxRetries) {
                        logger.log("MQTTManager", Logger::Level::ERROR, "Max retries reached. Resetting retry count.");
                        retryCount = 0;
                    }
                }
            }
            
            mqttClient.loop();
            processPublishBuffer();
            xSemaphoreGive(mqttMutex);
        }
        
        vTaskDelay(xFrequency);
    }

    vTaskDelete(NULL);
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

    return connected;
}

void ESPMQTTManager::disconnect() {
    if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
        mqttClient.disconnect();
        xSemaphoreGive(mqttMutex);
    }
    logger.log("MQTTManager", Logger::Level::INFO, "Disconnected from MQTT broker");
}

bool ESPMQTTManager::publish(const char* topic, const char* payload, bool retained) {
    PublishItem item = {String(topic), String(payload), retained};
    
    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(config.publishTimeout)) == pdTRUE) {
        if (mqttClient.connected()) {
            bool result = mqttClient.publish(topic, payload, retained);
            xSemaphoreGive(mqttMutex);
            if (result) {
                logger.log("MQTTManager", Logger::Level::INFO, "Published to topic: %s", topic);
            } else {
                logger.log("MQTTManager", Logger::Level::ERROR, "Failed to publish to topic: %s", topic);
            }
            return result;
        }
        xSemaphoreGive(mqttMutex);
    }
    
    // If we couldn't publish immediately, add to buffer
    if (xQueueSend(publishBuffer, &item, 0) != pdTRUE) {
        logger.log("MQTTManager", Logger::Level::ERROR, "Failed to add publish message to buffer. Buffer full.");
        return false;
    }
    
    logger.log("MQTTManager", Logger::Level::INFO, "Added publish message to buffer for topic: %s", topic);
    return true;
}

void ESPMQTTManager::processPublishBuffer() {
    PublishItem item;
    while (xQueueReceive(publishBuffer, &item, 0) == pdTRUE) {
        if (mqttClient.connected()) {
            bool result = mqttClient.publish(item.topic.c_str(), item.payload.c_str(), item.retained);
            if (result) {
                logger.log("MQTTManager", Logger::Level::INFO, "Published buffered message to topic: %s", item.topic.c_str());
            } else {
                logger.log("MQTTManager", Logger::Level::ERROR, "Failed to publish buffered message to topic: %s", item.topic.c_str());
                // Re-add to queue if publish failed
                if (xQueueSend(publishBuffer, &item, 0) != pdTRUE) {
                    logger.log("MQTTManager", Logger::Level::ERROR, "Failed to re-add publish message to buffer. Buffer full.");
                }
                break;  // Stop processing if we couldn't publish
            }
        } else {
            // Re-add to queue if not connected
            if (xQueueSend(publishBuffer, &item, 0) != pdTRUE) {
                logger.log("MQTTManager", Logger::Level::ERROR, "Failed to re-add publish message to buffer. Buffer full.");
            }
            break;  // Stop processing if not connected
        }
    }
}

bool ESPMQTTManager::subscribe(const char* topic, uint8_t qos) {
    bool result = false;
    if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
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
    }
    return result;
}

bool ESPMQTTManager::isConnected() {
    bool connected = false;
    if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
        connected = mqttClient.connected();
        xSemaphoreGive(mqttMutex);
    }
    return connected;
}

void ESPMQTTManager::setCallback(MQTT_CALLBACK_SIGNATURE) {
    if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
        mqttClient.setCallback(callback);
        xSemaphoreGive(mqttMutex);
    }
    logger.log("MQTTManager", Logger::Level::INFO, "MQTT callback set");
}

void ESPMQTTManager::setAuthMode(AuthMode mode) {
    config.authMode = mode;
    logger.log("MQTTManager", Logger::Level::INFO, "Authentication mode updated");
}

PubSubClient& ESPMQTTManager::getClient() {
    return mqttClient;
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