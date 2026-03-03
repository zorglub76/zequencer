/* components/ui/ui.c — stub */

#include "ui.h"
#include "sequencer.h"
#include "esp_log.h"

static const char *TAG = "ui";

esp_err_t ui_init(void)
{
    ESP_LOGI(TAG, "UI init (stub — GPIO/encoder implementation TBD)");
    /* TODO: configure GPIO ISR for buttons, rotary encoder for BPM */
    return ESP_OK;
}