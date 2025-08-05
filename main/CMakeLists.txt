set(srcs
    "main.c"
    "sketch.cpp"
    "BatteryMonitor.cpp"
    "ConfigManager.cpp"
    "LEDController.cpp"
    "MotorController.cpp"
    "ServoController.cpp"
    "SystemMonitor.cpp"
    "TinkerThinkerBoard.cpp"
    "WebServerManager.cpp"
)

set(includes
    "."
)

set(requires
    bluepad32
    bluepad32_arduino
    arduino
    btstack
    AsyncTCP
    ESPAsyncWebServer
    ArduinoJson
    FastLED
    ESP32Servo
)

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS ${includes}
                       REQUIRES ${requires})
