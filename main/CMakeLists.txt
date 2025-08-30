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
    "InputBindingManager.cpp"
)

set(includes
    "."
)

set(requires
    bluepad32
    bluepad32_arduino
    espressif__arduino-esp32
    btstack
    ESPAsyncWebServer
    bblanchon__arduinojson
    FastLED
    ESP32Servo
)

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS ${includes}
                       REQUIRES ${requires})

# Ensure FastLED UI headers are not required during IDF builds
target_compile_definitions(${COMPONENT_LIB} PRIVATE FASTLED_LEAN_AND_MEAN=1)
