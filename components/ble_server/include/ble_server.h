/* components/ble_server/include/ble_server.h
 *
 * BLE GATT server — exposes the Zequencer control service to the mobile app.
 *
 * Planned GATT characteristics:
 *   ┌─────────────────────────────┬──────────┬───────────────────────────────┐
 *   │ Characteristic              │ Props    │ Description                   │
 *   ├─────────────────────────────┼──────────┼───────────────────────────────┤
 *   │ ZEQ_CHAR_PATTERN            │ W        │ Write full pattern (protobuf)  │
 *   │ ZEQ_CHAR_TRANSPORT          │ W        │ Start / Stop / Reset          │
 *   │ ZEQ_CHAR_BPM               │ W        │ Set BPM (uint16 LE)           │
 *   │ ZEQ_CHAR_POSITION           │ N (50ms) │ Current step index (uint8)    │
 *   └─────────────────────────────┴──────────┴───────────────────────────────┘
 *
 * The position characteristic sends a BLE notification every
 * CONFIG_ZEQ_BLE_POSITION_INTERVAL_MS milliseconds so the mobile app
 * can animate a step cursor in near-real-time.
 */

#pragma once

#include "sequencer.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise and start the BLE GATT server.
 *
 * @param cbs  Sequencer control callbacks wired to incoming BLE writes.
 */
esp_err_t ble_server_init(const seq_control_callbacks_t *cbs);

/** Stop advertising and disconnect any connected client. */
void ble_server_deinit(void);

#ifdef __cplusplus
}
#endif