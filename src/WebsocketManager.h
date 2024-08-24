#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <memory>
#include "ESPLogger.h"
#include "esp_timer.h"

class WebSocketManager {
public:
    WebSocketManager(AsyncWebServer& server) : ws(new AsyncWebSocket("/ws")) {
        server.addHandler(ws.get());
        ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
                           void* arg, uint8_t* data, size_t len) {
            handleWebSocketEvent(server, client, type, arg, data, len);
        });

        // Set up a timer for periodic pings
        esp_timer_create_args_t timerArgs;
        timerArgs.callback = pingTimerCallback;
        timerArgs.arg = this;
        timerArgs.name = "websocket_ping";
        timerArgs.dispatch_method = ESP_TIMER_TASK;

        esp_timer_create(&timerArgs, &pingTimer);
        esp_timer_start_periodic(pingTimer, 60000000); // 60 seconds in microseconds
    }

    ~WebSocketManager() {
        esp_timer_stop(pingTimer);
        esp_timer_delete(pingTimer);
    }

    void handleLog(std::string_view tag, Logger::Level level, std::string_view message) {
        if (ws->count() > 0) {
            JsonDocument doc;
            doc["type"] = "log";
            doc["tag"] = tag;
            doc["level"] = static_cast<int>(level);  // Changed: Use integer representation
            doc["message"] = message;

            String json;
            serializeJson(doc, json);
            ws->textAll(json);
        }
    }

private:
    std::unique_ptr<AsyncWebSocket> ws;
    esp_timer_handle_t pingTimer;
    std::vector<uint32_t> activeClients;

    static void pingTimerCallback(void* arg) {
        WebSocketManager* self = static_cast<WebSocketManager*>(arg);
        self->pingClients();
    }

    void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
                              void* arg, uint8_t* data, size_t len) {
        switch (type) {
            case WS_EVT_CONNECT:
                Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
                activeClients.push_back(client->id());
                break;
            case WS_EVT_DISCONNECT:
                Serial.printf("WebSocket client #%u disconnected\n", client->id());
                activeClients.erase(std::remove(activeClients.begin(), activeClients.end(), client->id()), activeClients.end());
                break;
            case WS_EVT_DATA:
                handleWebSocketMessage(arg, data, len);
                break;
            case WS_EVT_PONG:
                Serial.printf("Received pong from client #%u\n", client->id());
                break;
            case WS_EVT_ERROR:
                Serial.printf("WebSocket error %u: %s\n", *((uint16_t*)arg), (char*)data);
                break;
        }
    }

    void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0;
            String message = (char*)data;
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, message);
            if (error) {
                Serial.print("deserializeJson() failed: ");
                Serial.println(error.c_str());
                return;
            }

            if (doc["type"] == "ping") {
                // Respond to ping with a pong
                JsonDocument pongDoc;
                pongDoc["type"] = "pong";
                String pongMessage;
                serializeJson(pongDoc, pongMessage);
                ws->textAll(pongMessage);
            }
        }
    }

    void pingClients() {
        JsonDocument doc;
        doc["type"] = "ping";
        String pingMessage;
        serializeJson(doc, pingMessage);
        
        for (auto it = activeClients.begin(); it != activeClients.end();) {
            AsyncWebSocketClient* client = ws->client(*it);
            if (client && client->status() == WS_CONNECTED) {
                client->text(pingMessage);
                ++it;
            } else {
                it = activeClients.erase(it);
            }
        }
        Serial.printf("Sent ping to %u active clients\n", activeClients.size());
    }

    // const char* getLevelString(Logger::Level level) {
    //     switch (level) {
    //         case Logger::Level::DEBUG: return "DEBUG";
    //         case Logger::Level::INFO: return "INFO";
    //         case Logger::Level::WARNING: return "WARNING";
    //         case Logger::Level::ERROR: return "ERROR";
    //         default: return "UNKNOWN";
    //     }
    // }
};

#endif // WEBSOCKET_MANAGER_H