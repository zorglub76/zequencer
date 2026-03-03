/* components/ble_server/ble_server.c
 *
 * BLE GATT server — stub ready for NimBLE implementation.
 *
 * TODO (Stage 2):
 *   - Define GATT service and characteristic UUIDs
 *   - Implement NimBLE host/controller init
 *   - Implement write handlers (pattern, transport, BPM)
 *   - Start position-notification task (CONFIG_ZEQ_BLE_POSITION_INTERVAL_MS)
 */

#include "ble_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ble_server";

/* Stored reference to sequencer callbacks */
static const seq_control_callbacks_t *s_seq_cbs = NULL;

/* ── Position notification task ─────────────────────────────────────────── */
static void position_notify_task(void *arg)
{
    const TickType_t interval =
        pdMS_TO_TICKS(CONFIG_ZEQ_BLE_POSITION_INTERVAL_MS);

    for (;;) {
        vTaskDelay(interval);

        uint8_t step = s_seq_cbs->get_current_step();

        /* TODO: send BLE notification with 'step' value to connected client */
        ESP_LOGD(TAG, "Position notify: step %d", step);
    }
}

/* ── Public API ─────────────────────────────────────────────────────────── */

esp_err_t ble_server_init(const seq_control_callbacks_t *cbs)
{
    ESP_LOGI(TAG, "BLE server init (stub — full NimBLE implementation TBD)");

    s_seq_cbs = cbs;

    /* TODO: nimble_port_init(), ble_hs_cfg setup, gatt_svr_init(), etc. */

    /* Start position notification task */
    xTaskCreate(position_notify_task, "ble_pos_notify", 2048, NULL, 5, NULL);

    return ESP_OK;
}

void ble_server_deinit(void)
{
    /* TODO: nimble_port_stop(), nimble_port_deinit() */
    ESP_LOGI(TAG, "BLE server deinit");
}