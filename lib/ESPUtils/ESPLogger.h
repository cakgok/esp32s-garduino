#ifndef ESP_LOGGER_H
#define ESP_LOGGER_H

#include <array>
#include <string>
#include <string_view>
#include <functional>
#include <esp_log.h>
#include <mutex>
#include <atomic>
#include <cstdarg>
#include <vector>
#include <type_traits>
#include <ArduinoJson.h>

#if defined(ENABLE_SERIAL_PRINT) && !defined(ESP_LOGGER_SERIAL_PRINT)
#define ESP_LOGGER_SERIAL_PRINT
#endif

#ifdef ESP_LOGGER_SERIAL_PRINT
#include <Arduino.h>
#endif

class Logger {
public:
    enum class Level { DEBUG, INFO, WARNING, ERROR };

    static constexpr size_t MAX_LOGS = 100;
    static constexpr size_t LOG_SIZE = 156;
    static constexpr size_t TAG_SIZE = 20;
    static constexpr std::string_view DEFAULT_TAG = "DEFAULT";
    static constexpr std::string_view OVERFLOW_MSG = " [LOG OVERFLOW]";

    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    void setCallback(std::function<void(std::string_view, Level, std::string_view)> cb) {
        std::lock_guard<std::mutex> lock(logMutex);
        callback = std::move(cb);
    }

    void addLogObserver(std::function<void(std::string_view, Level, std::string_view)> observer) {
        std::lock_guard<std::mutex> lock(logMutex);
        observers.push_back(std::move(observer));
    }

    void setFilterLevel(Level level) {
        filterLevel.store(level, std::memory_order_relaxed);
    }

    template<typename... Args>
    void log(std::string_view tag, Level level, const char* format, Args&&... args) {
        if (level >= filterLevel.load(std::memory_order_relaxed)) {
            char message[LOG_SIZE];
            snprintf(message, sizeof(message), format, std::forward<Args>(args)...);
            addLog(tag, level, message);
        }
    }

    void log(std::string_view tag, Level level, const char* message) {
        if (level >= filterLevel.load(std::memory_order_relaxed)) {
            addLog(tag, level, message);
        }
    }

    struct LogEntry {
        char tag[TAG_SIZE];
        Level level;
        char message[LOG_SIZE];
    };

    bool getNextLog(LogEntry& entry) {
        std::lock_guard<std::mutex> lock(logMutex);
        if (count.load(std::memory_order_relaxed) == 0) {
            return false;
        }

        size_t currentTail = tail.load(std::memory_order_relaxed);
        memcpy(&entry, &buffer[currentTail], sizeof(LogEntry));
        tail.store((currentTail + 1) % MAX_LOGS, std::memory_order_relaxed);
        count.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

    String getNextLogJson() {
        LogEntry entry;
        if (getNextLog(entry)) {
            JsonDocument doc;
            doc["tag"] = entry.tag;
            doc["level"] = static_cast<int>(entry.level);
            doc["message"] = entry.message;

            String jsonString;
            serializeJson(doc, jsonString);
            return jsonString;
        }
        return String(""); // Empty string if no more logs
    }

    bool peekNextLog(LogEntry& entry, size_t offset = 0) {
        std::lock_guard<std::mutex> lock(logMutex);
        if (count.load(std::memory_order_relaxed) == 0) {
            return false;
        }

        size_t index = (firstLogIndex + offset) % MAX_LOGS;
        memcpy(&entry, &buffer[index], sizeof(LogEntry));
        return true;
    }

    String peekNextLogJson(size_t offset = 0) {
        LogEntry entry;
        if (peekNextLog(entry, offset)) {
            JsonDocument doc;
            doc["tag"] = entry.tag;
            doc["level"] = static_cast<int>(entry.level);
            doc["message"] = entry.message;

            String jsonString;
            serializeJson(doc, jsonString);
            return jsonString;
        }
        return String(""); // Empty string if no more logs
    }

    size_t getValidLogCount() const {
        return std::min(count.load(std::memory_order_relaxed), MAX_LOGS);
    }

    size_t getLogCount() const {
        return count.load(std::memory_order_relaxed);
    }


private:
    std::array<LogEntry, MAX_LOGS> buffer;
    std::atomic<size_t> head{0};
    std::atomic<size_t> tail{0};
    std::atomic<size_t> count{0};
    std::atomic<size_t> firstLogIndex{0};
    std::function<void(std::string_view, Level, std::string_view)> callback;
    std::vector<std::function<void(std::string_view, Level, std::string_view)>> observers;
    mutable std::mutex logMutex;
    std::atomic<Level> filterLevel{Level::DEBUG};

    Logger() {}

    void addLog(std::string_view tag, Level level, const char* message) {
        if (level >= filterLevel.load(std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> lock(logMutex);
            
            size_t currentHead = head.load(std::memory_order_relaxed);
            LogEntry& entry = buffer[currentHead];
            
            // Copy tag (with null termination)
            strncpy(entry.tag, tag.data(), TAG_SIZE - 1);
            entry.tag[TAG_SIZE - 1] = '\0';
            entry.level = level;

            // Copy message with potential overflow handling
            size_t messageLen = strlen(message);
            if (messageLen >= LOG_SIZE) {
                // Message is too long, truncate and append overflow message
                strncpy(entry.message, message, LOG_SIZE - OVERFLOW_MSG.length() - 1);
                strcpy(entry.message + LOG_SIZE - OVERFLOW_MSG.length() - 1, OVERFLOW_MSG.data());
            } else {
                // Message fits, copy as is
                strcpy(entry.message, message);
            }
            entry.message[LOG_SIZE - 1] = '\0';

            head.store((currentHead + 1) % MAX_LOGS, std::memory_order_relaxed);
            size_t currentCount = count.load(std::memory_order_relaxed);
            if (currentCount < MAX_LOGS) {
                count.store(currentCount + 1, std::memory_order_relaxed);
            } else {
                // Buffer is full, move firstLogIndex
                firstLogIndex.store((firstLogIndex.load(std::memory_order_relaxed) + 1) % MAX_LOGS, std::memory_order_relaxed);
            }

            if (callback) {
                callback(entry.tag, entry.level, entry.message);
            }

            for (const auto& observer : observers) {
                observer(entry.tag, entry.level, entry.message);
            }

            #ifdef ESP_LOGGER_SERIAL_PRINT
            const char* levelStr;
            switch (level) {
                case Level::DEBUG: levelStr = "DEBUG"; break;
                case Level::INFO: levelStr = "INFO"; break;
                case Level::WARNING: levelStr = "WARNING"; break;
                case Level::ERROR: levelStr = "ERROR"; break;
                default: levelStr = "UNKNOWN"; break;
            }
            Serial.printf("[%s] %s: %s\n", entry.tag, levelStr, entry.message);
            #endif
        }
    }
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
};

using LogLevel = Logger::Level;

#endif // ESP_LOGGER_H
