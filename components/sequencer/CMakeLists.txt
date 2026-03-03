idf_component_register(
    SRCS
        "sequencer.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        midi
        esp_timer
        driver          # gptimer lives here in IDF ≥ v5
        freertos
)