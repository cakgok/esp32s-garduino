// Add a maximum limit (~5) to websockeet connections, 
// each connection take ~10kb of stack space and leads to a crash eventually
#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <memory>
#include <map>
#include <string>
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

        // Set up a timer for periodic pings and cleanup
        esp_timer_create_args_t timerArgs;
        timerArgs.callback = periodicTaskCallback;
        timerArgs.arg = this;
        timerArgs.name = "websocket_periodic";
        timerArgs.dispatch_method = ESP_TIMER_TASK;

        esp_timer_create(&timerArgs, &periodicTimer);
        esp_timer_start_periodic(periodicTimer, 60000000); // 60 seconds in microseconds
    }

    ~WebSocketManager() {
        esp_timer_stop(periodicTimer);
        esp_timer_delete(periodicTimer);
    }

    void handleLog(std::string_view tag, Logger::Level level, std::string_view message) {
        if (ws->count() > 0) {
            JsonDocument doc;
            doc["type"] = "log";
            doc["tag"] = tag;
            doc["level"] = static_cast<int>(level);
            doc["message"] = message;

            String json;
            serializeJson(doc, json);
            ws->textAll(json);
        }
    }

private:
    struct ClientInfo {
        IPAddress ip;
        String connectionId;
        bool isPaused;
        uint32_t lastActivity;
    };

    std::unique_ptr<AsyncWebSocket> ws;
    esp_timer_handle_t periodicTimer;
    std::map<uint32_t, ClientInfo> clientMap;
    const size_t MAX_CONNECTIONS_PER_IP = 3;
    const uint32_t CLIENT_TIMEOUT = 300000; // 5 minutes in milliseconds

    static void periodicTaskCallback(void* arg) {
        WebSocketManager* self = static_cast<WebSocketManager*>(arg);
        self->pingClients();
        self->cleanupInactiveClients();
    }

    void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
                              void* arg, uint8_t* data, size_t len) {
        switch (type) {
            case WS_EVT_CONNECT:
                {
                    // Extract connection ID from URL parameters
                    String url = server->url();
                    int idStart = url.indexOf("id=");
                    String connectionId = (idStart != -1) ? url.substring(idStart + 3) : String(client->id());
                    
                    handleNewConnection(client, connectionId);
                }
                break;
            case WS_EVT_DISCONNECT:
                handleDisconnection(client);
                break;
            case WS_EVT_DATA:
                handleWebSocketMessage(client, arg, data, len);
                break;
            case WS_EVT_PONG:
                updateClientActivity(client);
                break;
            case WS_EVT_ERROR:
                Serial.printf("WebSocket error %u: %s\n", *((uint16_t*)arg), (char*)data);
                break;
        }
    }

    void handleNewConnection(AsyncWebSocketClient* client, const String& connectionId) {
        IPAddress clientIP = client->remoteIP();

        // Check if max connections per IP is reached
        size_t connectionsFromIP = countConnectionsFromIP(clientIP);
        if (connectionsFromIP >= MAX_CONNECTIONS_PER_IP) {
            client->close(1000, "Too many connections from this IP");
            return;
        }

        // Add new client to the map
        clientMap[client->id()] = {clientIP, connectionId, false, millis()};
        Serial.printf("WebSocket client #%u connected from %s with ID %s\n", 
                      client->id(), clientIP.toString().c_str(), connectionId.c_str());
    }

    void handleDisconnection(AsyncWebSocketClient* client) {
        clientMap.erase(client->id());
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    }

    void handleWebSocketMessage(AsyncWebSocketClient* client, void* arg, uint8_t* data, size_t len) {
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

            String type = doc["type"];
            if (type == "ping") {
                sendPong(client);
            } else if (type == "pause") {
                pauseClient(client);
            } else if (type == "resume") {
                resumeClient(client);
            }

            updateClientActivity(client);
        }
    }

    void sendPong(AsyncWebSocketClient* client) {
        JsonDocument pongDoc;
        pongDoc["type"] = "pong";
        String pongMessage;
        serializeJson(pongDoc, pongMessage);
        client->text(pongMessage);
    }

    void pauseClient(AsyncWebSocketClient* client) {
        auto it = clientMap.find(client->id());
        if (it != clientMap.end()) {
            it->second.isPaused = true;
        }
    }

    void resumeClient(AsyncWebSocketClient* client) {
        auto it = clientMap.find(client->id());
        if (it != clientMap.end()) {
            it->second.isPaused = false;
        }
    }

    void updateClientActivity(AsyncWebSocketClient* client) {
        auto it = clientMap.find(client->id());
        if (it != clientMap.end()) {
            it->second.lastActivity = millis();
        }
    }

    void pingClients() {
        JsonDocument doc;
        doc["type"] = "ping";
        String pingMessage;
        serializeJson(doc, pingMessage);
        
        for (auto& pair : clientMap) {
            if (!pair.second.isPaused) {
                AsyncWebSocketClient* client = ws->client(pair.first);
                if (client) {
                    client->text(pingMessage);
                }
            }
        }
    }

    void cleanupInactiveClients() {
        uint32_t now = millis();
        for (auto it = clientMap.begin(); it != clientMap.end();) {
            if (now - it->second.lastActivity > CLIENT_TIMEOUT) {
                AsyncWebSocketClient* client = ws->client(it->first);
                if (client) {
                    client->close(1000, "Timeout");
                }
                it = clientMap.erase(it);
            } else {
                ++it;
            }
        }
    }

    size_t countConnectionsFromIP(const IPAddress& ip) {
        size_t count = 0;
        for (const auto& pair : clientMap) {
            if (pair.second.ip == ip) {
                count++;
            }
        }
        return count;
    }

    String getConnectionId(AsyncWebSocketClient* client) {
        auto it = clientMap.find(client->id());
        if (it != clientMap.end()) {
            return it->second.connectionId;
        }
        return String(client->id()); // Fallback to client ID if no custom ID stored
    }
};

#endif // WEBSOCKET_MANAGER_H