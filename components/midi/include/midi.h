/* components/midi/include/midi.h
 *
 * MIDI output over UART at 31250 baud (DIN-5 / TRS MIDI standard).
 *
 * ISR-safe variants (midi_note_on_from_isr) post to a FreeRTOS queue;
 * a dedicated high-priority task drains the queue and writes to UART,
 * keeping the ISR execution time minimal.
 */

#pragma once

#include "sdkconfig.h"           // project configuration macros

/* default values in case sdkconfig isn't generated yet */
#ifndef CONFIG_ZEQ_MIDI_UART_PORT
#define CONFIG_ZEQ_MIDI_UART_PORT 1
#endif
#ifndef CONFIG_ZEQ_MIDI_TX_GPIO
#define CONFIG_ZEQ_MIDI_TX_GPIO 17
#endif

#include <stdint.h>
#include "esp_err.h"
#include "esp_attr.h"  // for IRAM_ATTR

#ifdef __cplusplus
extern "C" {
#endif

/* ── MIDI message status bytes ─────────────────────────────────────────── */
#define MIDI_NOTE_OFF       0x80
#define MIDI_NOTE_ON        0x90
#define MIDI_CC             0xB0
#define MIDI_ALL_NOTES_OFF  0x7B   /* CC value for All Notes Off */

/* ── Public API ─────────────────────────────────────────────────────────── */

/**
 * @brief Initialise the MIDI UART and start the sender task.
 *
 * Uses CONFIG_ZEQ_MIDI_UART_PORT and CONFIG_ZEQ_MIDI_TX_GPIO from Kconfig.
 */
esp_err_t midi_init(void);

/**
 * @brief Send a Note-On message (call from normal task context).
 *
 * @param channel  MIDI channel, 0-based (0 = channel 1).
 * @param note     MIDI note number (0–127).
 * @param velocity Note velocity (1–127); 0 is treated as Note-Off by spec.
 */
void midi_note_on(uint8_t channel, uint8_t note, uint8_t velocity);

/**
 * @brief Send a Note-On message — ISR-safe (posts to queue, no blocking).
 */
void IRAM_ATTR midi_note_on_from_isr(uint8_t channel, uint8_t note,
                                      uint8_t velocity);

/**
 * @brief Send a Note-Off message (call from normal task context).
 */
void midi_note_off(uint8_t channel, uint8_t note, uint8_t velocity);

/**
 * @brief Send MIDI CC 123 — All Notes Off on @p channel.
 */
void midi_all_notes_off(uint8_t channel);

#ifdef __cplusplus
}
#endif