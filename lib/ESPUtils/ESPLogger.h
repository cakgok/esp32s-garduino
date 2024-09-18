#ifndef ESP_LOGGER_H
#define ESP_LOGGER_H

#include <array>
#include <string>
#include <string_view>
#include <functional>
#include <mutex>
#include <atomic>
#include <vector>
#include <ArduinoJson.h>

#ifdef ENABLE_SERIAL_PRINT
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

    static Logger& instance();

    void setCallback(std::function<void(std::string_view, Level, std::string_view)> cb);
    void addLogObserver(std::function<void(std::string_view, Level, std::string_view)> observer);
    void setFilterLevel(Level level);

    template<typename... Args>
    void log(std::string_view tag, Level level, const char* format, Args&&... args) {
        if (level >= filterLevel.load(std::memory_order_relaxed)) {
            char message[LOG_SIZE];
            if constexpr(sizeof...(args) == 0) {
                // No formatting needed, just use the format as the message
                addLog(tag, level, format);
            } else {
                // Formatting needed
                snprintf(message, sizeof(message), format, std::forward<Args>(args)...);
                addLog(tag, level, message);
            }
        }
    }  
    struct LogEntry {
        char tag[TAG_SIZE];
        Level level;
        char message[LOG_SIZE];
    };

    bool getNextLog(LogEntry& entry);
    String getNextLogJson();
    bool peekNextLog(LogEntry& entry, size_t offset = 0);
    String peekNextLogJson(size_t offset = 0);
    size_t getValidLogCount() const;
    size_t getLogCount() const;

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

    void addLog(std::string_view tag, Level level, const char* message);

    Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

};

using LogLevel = Logger::Level;

#endif // ESP_LOGGER_H