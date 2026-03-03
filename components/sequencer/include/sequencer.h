/* components/sequencer/include/sequencer.h
 *
 * Public API for the Zequencer step-sequencer engine.
 *
 * Architecture notes
 * ──────────────────
 * • A hardware GPTimer ISR advances the step pointer at the BPM-derived rate.
 * • Pattern data uses a DOUBLE-BUFFER so BLE/UI can write the "staging" buffer
 *   while the ISR safely reads the "active" buffer. An atomic flag signals when
 *   the staging buffer is ready to swap.
 * • The ISR is kept minimal: advance step, read note, post to a MIDI task queue.
 */

#pragma once

#include "sdkconfig.h"    /* project configuration macros */

/* fallback defaults if sdkconfig not yet generated */
#ifndef CONFIG_ZEQ_SEQ_NUM_STEPS
#define CONFIG_ZEQ_SEQ_NUM_STEPS 16
#endif
#ifndef CONFIG_ZEQ_SEQ_DEFAULT_BPM
#define CONFIG_ZEQ_SEQ_DEFAULT_BPM 120
#endif
#ifndef CONFIG_ZEQ_SEQ_DEFAULT_NOTE
#define CONFIG_ZEQ_SEQ_DEFAULT_NOTE 60
#endif
#ifndef CONFIG_ZEQ_MIDI_DEFAULT_CHANNEL
#define CONFIG_ZEQ_MIDI_DEFAULT_CHANNEL 1
#endif

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ────────────────────────────────────────────────────────────── */
#define SEQ_MAX_STEPS     CONFIG_ZEQ_SEQ_NUM_STEPS  /* from Kconfig, default 16 */
#define SEQ_NOTE_OFF      0xFF                       /* sentinel: step is silent */

/* ── Data types ───────────────────────────────────────────────────────────── */

/** One step in the pattern. */
typedef struct {
    uint8_t  note;       /**< MIDI note number (0–127), or SEQ_NOTE_OFF       */
    uint8_t  velocity;   /**< MIDI velocity (1–127)                           */
    uint8_t  length_pct; /**< Note length as % of step duration (1–100)       */
    bool     active;     /**< true = this step triggers a note                 */
} seq_step_t;

/** Full pattern (one bank of steps). */
typedef struct {
    seq_step_t steps[SEQ_MAX_STEPS];
    uint8_t    num_steps;  /**< Active length (≤ SEQ_MAX_STEPS)               */
    uint16_t   bpm;        /**< Tempo for this pattern                        */
} seq_pattern_t;

/** Callbacks provided to the BLE / UI layers so they can control the engine. */
typedef struct {
    void (*set_bpm)(uint16_t bpm);
    void (*set_pattern)(const seq_pattern_t *pattern); /**< writes staging buf */
    void (*start)(void);
    void (*stop)(void);
    uint8_t (*get_current_step)(void);                 /**< for BLE feedback   */
} seq_control_callbacks_t;

/* ── Public API ───────────────────────────────────────────────────────────── */

/**
 * @brief Initialise the sequencer engine.
 *
 * Allocates the double buffer, configures the GPTimer, and loads default
 * pattern from Kconfig values. Does NOT start the timer.
 */
esp_err_t sequencer_init(void);

/** Start the hardware timer — steps begin advancing immediately. */
void sequencer_start(void);

/** Stop the hardware timer and send MIDI All-Notes-Off. */
void sequencer_stop(void);

/** Update BPM and reprogram the timer period. Safe to call while running. */
void sequencer_set_bpm(uint16_t bpm);

/**
 * @brief Stage a new pattern for the next bar boundary.
 *
 * Copies @p pattern into the staging buffer and sets the swap-pending flag.
 * The ISR will atomically swap buffers at the start of the next bar.
 *
 * @param pattern Pointer to the new pattern (caller retains ownership).
 */
void sequencer_set_pattern(const seq_pattern_t *pattern);

/** Return the index of the step currently being played (0-based). */
uint8_t sequencer_get_current_step(void);

/**
 * @brief Return a struct of function pointers for controlling the sequencer.
 *
 * Used by BLE server and UI to avoid direct coupling to implementation details.
 */
const seq_control_callbacks_t *sequencer_get_control_callbacks(void);

#ifdef __cplusplus
}
#endif