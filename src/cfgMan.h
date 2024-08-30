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
    ConfigManager(PreferencesHandler& preferencesHandler) : logger(Logger::instance()), prefsHandler(preferencesHandler) {
        preferences.begin("config", false);
        initializeConfig();
    }
                    
    void initializeConfig();
    
    template<typename T>
    T get(ConfigKey key, size_t sensorIndex = 0);
    
    template<typename T>
    bool set(ConfigKey key, T value, size_t sensorIndex = 0);
    
    void loadCache();
    void saveCache();
    

private:
    mutable std::shared_mutex mutex;
    Preferences preferences;
    PreferencesHandler& prefsHandler;
    Logger& logger;
    static const std::map<ConfigKey, ConfigInfo> configMap;


    template<typename T>
    bool validate(ConfigKey key, T value);

};