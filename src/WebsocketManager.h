#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <memory>
#include "ESPLogger.h"

class WebSocketManager {
public:
    WebSocketManager(AsyncWebServer& server) : ws(new AsyncWebSocket("/ws")) {
        server.addHandler(ws.get());
        ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
                           void* arg, uint8_t* data, size_t len) {
            handleWebSocketEvent(server, client, type, arg, data, len);
        });
    }

    ~WebSocketManager() = default;

    void handleLog(std::string_view tag, Logger::Level level, std::string_view message) {
        if (ws->count() > 0) {
            JsonDocument doc;
            doc["tag"] = tag;
            doc["level"] = getLevelString(level);  // Add this function to convert level to string
            doc["message"] = message;

            String json;
            serializeJson(doc, json);
            ws->textAll(json);
        }
    }

private:
    std::unique_ptr<AsyncWebSocket> ws;

    void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
                              void* arg, uint8_t* data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            Serial.printf("WebSocket client #%u connected\n", client->id());
            // Optionally send all existing logs to the client upon connection
        } else if (type == WS_EVT_DISCONNECT) {
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
        }
    }

    const char* getLevelString(Logger::Level level) {
        switch (level) {
            case Logger::Level::DEBUG: return "DEBUG";
            case Logger::Level::INFO: return "INFO";
            case Logger::Level::WARNING: return "WARNING";
            case Logger::Level::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
};

#endif // WEBSOCKET_MANAGER_H
