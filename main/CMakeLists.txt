# zequencer/main/CMakeLists.txt
idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        sequencer
        midi
        ble_server
        ui
        nvs_flash
        esp_timer
)