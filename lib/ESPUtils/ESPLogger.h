#ifndef ESP_LOGGER_H
#define ESP_LOGGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class ESPLogger {
public:
   ESPLogger(size_t bufferSize = 50) 
        : currentLogLevel(LogLevel::INFO) {
        logBuffer = xRingbufferCreate(bufferSize, RINGBUF_TYPE_NOSPLIT);
        mutex = xSemaphoreCreateMutex();
    }

    ~ESPLogger() {
        if (logBuffer != nullptr) {
            vRingbufferDelete(logBuffer);
        }
        vSemaphoreDelete(mutex);
    }

    void log(const char* format, ...) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        va_list args;
        va_start(args, format);
        logInternal(LogLevel::INFO, format, args);
        va_end(args);
        xSemaphoreGive(mutex);
    }

    void log(LogLevel level, const char* format, ...) {
        if (level < currentLogLevel) return;
        xSemaphoreTake(mutex, portMAX_DELAY);
        va_list args;
        va_start(args, format);
        logInternal(level, format, args);
        va_end(args);
        xSemaphoreGive(mutex);
    }

    void setLogLevel(LogLevel level) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        currentLogLevel = level;
        xSemaphoreGive(mutex);
    }
    typedef std::function<void(const String& log)> LogCallback;

    void processLogs(LogCallback callback) {
        xSemaphoreTake(mutex, portMAX_DELAY); // Protect access to logBuffer
        if (logBuffer == nullptr) {
            xSemaphoreGive(mutex);
            return;
        }

        size_t item_size;
        char* item;

        while ((item = (char*) xRingbufferReceive(logBuffer, &item_size, pdMS_TO_TICKS(10))) != nullptr) {
            String logEntry = String(item);
            callback(logEntry);  // Process each log entry individually
            vRingbufferReturnItem(logBuffer, (void*) item);
        }
        
        xSemaphoreGive(mutex); // Release the mutex
    }

private:
    RingbufHandle_t logBuffer;
    LogLevel currentLogLevel;
    SemaphoreHandle_t mutex;

    void logInternal(LogLevel level, const char* format, va_list args) {
        String message;

        // Reserve enough space for the log level string, the formatted message, and additional characters (e.g., ": ")
        size_t levelStringLength = strlen(getLevelString(level));
        size_t bufferLength = 256; // Adjust based on your needs
        message.reserve(levelStringLength + bufferLength + 2); // +2 for ": " and null terminator

        // Construct the log message
        message += getLevelString(level);
        message += ": ";

        // Use vsnprintf to format the message into the String
        char buffer[bufferLength];
        vsnprintf(buffer, sizeof(buffer), format, args);
        message += buffer;

        // Send the message to the ring buffer
        if (logBuffer != nullptr) {
            xRingbufferSend(logBuffer, message.c_str(), message.length() + 1, pdMS_TO_TICKS(10));
        }

        Serial.println(message);
    }

    const char* getLevelString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
};

#endif // ESP_LOGGER_H
