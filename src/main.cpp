#include "setup.h"
#define DISABLE_SERIAL_PRINT

void setup() {
    Serial.begin(115200);                 
    logger.setLogLevel(LogLevel::INFO);
    setup_wifi();
    setupOTA(&logger);
    timeSetup.setup();
    mqttManager.setup();
    setupLittleFS();
    sensorManager.setupFloatSwitch(FLOAT_SWITCH_PIN);
    sensorManager.setupSensors(SDA_PIN, SCL_PIN);
    sensorManager.setupMoistureSensors(MOISTURE_SENSOR_PINS);
    webServer.begin();
    lcdManager.start();
    publishTask.start();

    logger.log("Setup complete");   
}

void loop() {
  ArduinoOTA.handle();
  mqttManager.loop();
  delay(10);
}

