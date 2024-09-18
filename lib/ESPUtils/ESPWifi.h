#ifndef ESPWIFI_H
#define ESPWIFI_H

#include <WiFi.h>
#include "ESPLogger.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ESPmDNS.h>

class WiFiWrapper {
private:
    const char* ssid;
    const char* password;
    Logger& logger;
    TaskHandle_t reconnectTaskHandle;
    static const uint32_t STACK_SIZE = 4096;
    static const UBaseType_t TASK_PRIORITY = 1;
    static const unsigned long RECONNECT_INTERVAL = 30000;

    IPAddress staticIP;
    IPAddress gateway;
    IPAddress subnet;
    String hostname;
    bool useStaticIP;

    static constexpr const char* LOG_TAG = "WiFi";

    bool connect();
    static void reconnectTask(void* pvParameters);
    void logConnectionDetails();
    IPAddress stringToIP(const String& ipString);

public:
    WiFiWrapper(const char* ssid, const char* password);
    void setStaticIP(const String& ip, const String& gateway = "", const String& subnet = "255.255.255.0");
    void setHostname(const String& hostname);
    bool begin();
    bool isConnected();
    IPAddress getLocalIP();
    String getHostname();
    bool setupMDNS(const char* hostname);
    ~WiFiWrapper();
};

#endif // ESPWIFI_H