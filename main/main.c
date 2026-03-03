/* zequencer/main/main.c
 *
 * Application entry point.
 * Initialises all subsystems in dependency order, then hands control
 * to FreeRTOS (each component starts its own task internally).
 */

#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"

#include "sequencer.h"
#include "midi.h"
#include "ble_server.h"
#include "ui.h"

static const char *TAG = "main";

/* ── Forward declarations ─────────────────────────────────────────────────── */
static void nvs_init(void);

/* ── Entry point ─────────────────────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "Zequencer starting up...");

    /* 1. Non-volatile storage — must be first; BLE and pattern save need it */
    nvs_init();

    /* 2. MIDI output — UART must be ready before the sequencer fires notes */
    midi_init();

    /* 3. Sequencer engine — registers hardware timer ISR, does NOT start yet */
    sequencer_init();

    /* 4. BLE server — advertises and waits for the controller app to connect.
     *    Passes a callback so the controller can start/stop/update the pattern. */
    ble_server_init(sequencer_get_control_callbacks());

    /* 5. UI — GPIO buttons/encoder for local control */
    ui_init();

    /* 6. Start sequencing — timer ISR begins firing */
    sequencer_start();

    ESP_LOGI(TAG, "All subsystems initialised. Sequencer running.");

    /* app_main returns here; FreeRTOS scheduler keeps everything alive */
}

/* ── Private helpers ─────────────────────────────────────────────────────── */
static void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition was truncated or is a new version — erase and retry */
        ESP_LOGW(TAG, "NVS needs erase, reflashing defaults");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG, "NVS initialised");
}