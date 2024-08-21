// ///////////////////////////
// Logger& logger = Logger::instance();

// // Basic logging
// logger.log("This is a default INFO log");
// logger.log(LogLevel::DEBUG, "This is a DEBUG log");
// logger.log("MYTAG", LogLevel::WARNING, "This is a WARNING log with a custom tag");

// // Logging with format strings
// int temperature = 25;
// float humidity = 60.5f;
// logger.log(LogLevel::INFO, "Temperature: %d°C, Humidity: %.1f%%", temperature, humidity);

// // Logging with string concatenation (using format string)
// std::string sensorName = "DHT22";
// logger.log(LogLevel::ERROR, "Sensor %s failed to initialize", sensorName.c_str());
// //////////////////////////////

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

#ifdef ENABLE_SERIAL_PRINT
#include <Arduino.h>
#endif
#define ENABLE_SERIAL_PRINT // Enable serial printing

class Logger {
public:
    enum class Level { DEBUG, INFO, WARNING, ERROR };

    static constexpr size_t MAX_LOGS = 100;
    static constexpr size_t LOG_SIZE = 512;
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

    // Overload for when there's no format string
    void log(std::string_view tag, Level level, const char* message) {
        if (level >= filterLevel.load(std::memory_order_relaxed)) {
            addLog(tag, level, message);
        }
    }

    [[nodiscard]] std::array<std::string_view, MAX_LOGS> getLogs() const {
        std::lock_guard<std::mutex> lock(logMutex);
        std::array<std::string_view, MAX_LOGS> result;
        size_t currentCount = count.load(std::memory_order_relaxed);
        size_t currentHead = head.load(std::memory_order_relaxed);
        size_t index = (currentHead - currentCount + MAX_LOGS) % MAX_LOGS;
        for (size_t i = 0; i < currentCount; i++) {
            result[i] = std::string_view(&buffer[index * LOG_SIZE], strnlen(&buffer[index * LOG_SIZE], LOG_SIZE));
            index = (index + 1) % MAX_LOGS;
        }
        return result;
    }

    struct LogEntry {
        const char* tag;
        Level level;
        const char* message;
    };

    static bool startsWith(const char* str, const char* prefix) {
        return strncmp(str, prefix, strlen(prefix)) == 0;
    }

    std::vector<LogEntry> getChronologicalLogs() const {
        std::lock_guard<std::mutex> lock(logMutex);
        std::vector<LogEntry> result;
        size_t currentCount = count.load(std::memory_order_relaxed);
        size_t currentHead = head.load(std::memory_order_relaxed);
        
        if (currentCount == 0) {
            return result;
        }
        
        result.reserve(currentCount);  // Optimize for performance
        
        size_t startIndex = (currentHead - currentCount + MAX_LOGS) % MAX_LOGS;
        for (size_t i = 0; i < currentCount; i++) {
            size_t index = (startIndex + i) % MAX_LOGS;
            const char* logEntry = &buffer[index * LOG_SIZE];
            
            // Parse the log entry
            const char* tagStart = logEntry + 1;  // Skip '['
            const char* tagEnd = strchr(tagStart, ']');
            if (tagEnd) {
                const char* messageStart = tagEnd + 2;  // Skip "] "
                
                // Determine log level
                Level level = Level::INFO;
                if (startsWith(messageStart, "ERROR:")) level = Level::ERROR;
                else if (startsWith(messageStart, "WARNING:")) level = Level::WARNING;
                else if (startsWith(messageStart, "DEBUG:")) level = Level::DEBUG;
                
                result.push_back({tagStart, level, messageStart});
            }
        }
        return result;
    }

private:
    std::array<char, MAX_LOGS * LOG_SIZE> buffer;
    std::atomic<size_t> head{0};
    std::atomic<size_t> count{0};
    std::function<void(std::string_view, Level, std::string_view)> callback;
    std::vector<std::function<void(std::string_view, Level, std::string_view)>> observers;
    mutable std::mutex logMutex;
    std::atomic<Level> filterLevel{Level::DEBUG};

    Logger() {}

        void addLog(std::string_view tag, Level level, const char* message) {
        if (level >= filterLevel.load(std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> lock(logMutex);
            
            size_t currentHead = head.load(std::memory_order_relaxed);
            char* entry = &buffer[currentHead * LOG_SIZE];
            
            const char* levelStr;
            switch (level) {
                case Level::DEBUG: levelStr = "DEBUG"; break;
                case Level::INFO: levelStr = "INFO"; break;
                case Level::WARNING: levelStr = "WARNING"; break;
                case Level::ERROR: levelStr = "ERROR"; break;
                default: levelStr = "UNKNOWN"; break;
            }
            
            int written = snprintf(entry, LOG_SIZE, "[%.*s] %s: %s", 
                                   static_cast<int>(tag.size()), tag.data(), levelStr, message);
            
            if (written >= LOG_SIZE) {
                strncpy(entry + LOG_SIZE - OVERFLOW_MSG.size() - 1, 
                        OVERFLOW_MSG.data(), OVERFLOW_MSG.size());
                entry[LOG_SIZE - 1] = '\0';
            }

            head.store((currentHead + 1) % MAX_LOGS, std::memory_order_relaxed);
            if (count.load(std::memory_order_relaxed) < MAX_LOGS) {
                count.fetch_add(1, std::memory_order_relaxed);
            }

            if (callback) {
                callback(tag, level, entry);
            }

            for (const auto& observer : observers) {
                observer(tag, level, entry);
            }

            #ifdef ENABLE_SERIAL_PRINT
            Serial.println(entry);
            #endif
        }
    }
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
};

using LogLevel = Logger::Level;

#endif // ESP_LOGGER_H