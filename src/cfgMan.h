#include <Preferences.h>
#include <map>
#include <string>
#include <functional>
#include <array>
#include <vector>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <variant>
#include "Globals.h"
#include "ESPLogger.h"
#include "PreferencesHandler.h"
#include "ConfigTypes.h"

class ConfigManager {
public:
    ConfigManager() : logger(Logger::instance()),
                    
    void init();
    
    template<typename T>
    T get(ConfigKey key, size_t sensorIndex = 0);
    
    template<typename T>
    bool set(ConfigKey key, T value, size_t sensorIndex = 0);
    
    void loadCache();
    void saveCache();
    

private:
    Logger& logger;
    Preferences preferences;
    std::map<ConfigKey, std::variant<int, float, bool, std::vector<int>, std::vector<bool>, std::vector<int64_t>>> cache;


    template<typename T>
    bool validate(ConfigKey key, T value);

};