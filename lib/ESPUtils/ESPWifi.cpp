#include "ESPWiFi.h"

WiFiWrapper::WiFiWrapper(const char* ssid, const char* password)
    : ssid(ssid), password(password), logger(Logger::instance()), reconnectTaskHandle(NULL), useStaticIP(false) {
    logger.log(LOG_TAG, LogLevel::DEBUG, "WiFiWrapper instance created");
}

bool WiFiWrapper::connect() {
    if (useStaticIP) {
        if (!WiFi.config(staticIP, gateway, subnet)) {
            logger.log(LOG_TAG, LogLevel::ERROR, "Failed to configure static IP");
            return false;
        }
    }

    if (!hostname.isEmpty()) {
        WiFi.setHostname(hostname.c_str());
    }

    logger.log(LOG_TAG, LogLevel::INFO, "Connecting to WiFi SSID: %s", ssid);
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        attempts++;
        logger.log(LOG_TAG, LogLevel::DEBUG, "WiFi connection attempt: %d", attempts);
    }
    return WiFi.status() == WL_CONNECTED;
}

void WiFiWrapper::reconnectTask(void* pvParameters) {
    WiFiWrapper* wifiWrapper = static_cast<WiFiWrapper*>(pvParameters);
    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            wifiWrapper->logger.log(LOG_TAG, LogLevel::WARNING, "WiFi disconnected. Attempting to reconnect...");
            if (wifiWrapper->connect()) {
                wifiWrapper->logger.log(LOG_TAG, LogLevel::INFO, "WiFi reconnected successfully");
                wifiWrapper->logConnectionDetails();
            } else {
                wifiWrapper->logger.log(LOG_TAG, LogLevel::ERROR, "WiFi reconnection failed");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(RECONNECT_INTERVAL));
    }
}

void WiFiWrapper::logConnectionDetails() {
    logger.log(LOG_TAG, LogLevel::INFO, "Connection Details:");
    logger.log(LOG_TAG, LogLevel::INFO, "IP: %s", WiFi.localIP().toString().c_str());
    logger.log(LOG_TAG, LogLevel::INFO, "Gateway: %s", WiFi.gatewayIP().toString().c_str());
    logger.log(LOG_TAG, LogLevel::INFO, "Subnet: %s", WiFi.subnetMask().toString().c_str());
    logger.log(LOG_TAG, LogLevel::INFO, "DNS: %s", WiFi.dnsIP().toString().c_str());
    logger.log(LOG_TAG, LogLevel::INFO, "Hostname: %s", WiFi.getHostname());
    logger.log(LOG_TAG, LogLevel::INFO, "MAC: %s", WiFi.macAddress().c_str());
    logger.log(LOG_TAG, LogLevel::INFO, "SSID: %s", WiFi.SSID().c_str());
    logger.log(LOG_TAG, LogLevel::INFO, "RSSI: %d dBm", WiFi.RSSI());
}

IPAddress WiFiWrapper::stringToIP(const String& ipString) {
    IPAddress ip;
    if (ip.fromString(ipString)) {
        return ip;
    } else {
        logger.log(LOG_TAG, LogLevel::ERROR, "Invalid IP address format: %s", ipString.c_str());
        return IPAddress(INADDR_NONE);  // Return an invalid IP address
    }
}

void WiFiWrapper::setStaticIP(const String& ip, const String& gateway, const String& subnet) {
    staticIP = stringToIP(ip);
    this->gateway = gateway.isEmpty() ? IPAddress(staticIP[0], staticIP[1], staticIP[2], 1) : stringToIP(gateway);
    this->subnet = stringToIP(subnet);
    useStaticIP = true;

    logger.log(LOG_TAG, LogLevel::INFO, "Static IP set: %s", ip.c_str());
    logger.log(LOG_TAG, LogLevel::INFO, "Gateway: %s", this->gateway.toString().c_str());
    logger.log(LOG_TAG, LogLevel::INFO, "Subnet: %s", this->subnet.toString().c_str());
}

void WiFiWrapper::setHostname(const String& hostname) {
    this->hostname = hostname;
    logger.log(LOG_TAG, LogLevel::INFO, "Hostname set: %s", hostname.c_str());
}

bool WiFiWrapper::begin() {
    logger.log(LOG_TAG, LogLevel::INFO, "Initializing WiFi connection");
    WiFi.mode(WIFI_STA);
    bool connected = connect();
    if (connected) {
        logger.log(LOG_TAG, LogLevel::INFO, "WiFi connected successfully");
        logConnectionDetails();
    } else {
        logger.log(LOG_TAG, LogLevel::ERROR, "Initial WiFi connection failed, but reconnection task is running");
    }

    xTaskCreate(
        reconnectTask,
        "WiFiReconnect",
        STACK_SIZE,
        this,
        TASK_PRIORITY,
        &reconnectTaskHandle
    );
    logger.log(LOG_TAG, LogLevel::DEBUG, "WiFi reconnection task created");

    return connected;
}

bool WiFiWrapper::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

IPAddress WiFiWrapper::getLocalIP() {
    return WiFi.localIP();
}

String WiFiWrapper::getHostname() {
    return WiFi.getHostname();
}

bool WiFiWrapper::setupMDNS(const char* hostname) {
    if (MDNS.begin(hostname)) {
        logger.log(LOG_TAG, LogLevel::INFO, "mDNS responder started. Hostname: %s.local", hostname);
        return true;
    } else {
        logger.log(LOG_TAG, LogLevel::ERROR, "Error setting up mDNS responder!");
        return false;
    }
}

WiFiWrapper::~WiFiWrapper() {
    if (reconnectTaskHandle != NULL) {
        vTaskDelete(reconnectTaskHandle);
        logger.log(LOG_TAG, LogLevel::DEBUG, "WiFi reconnection task deleted");
    }
}