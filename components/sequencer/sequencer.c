/* components/sequencer/sequencer.c
 *
 * Sequencer engine implementation.
 *
 * Timer ISR (IRAM-safe, minimal work):
 *   1. Check swap-pending flag → swap buffers if set.
 *   2. Read active step from active buffer.
 *   3. Post MIDI note event to midi_task queue.
 *   4. Advance step counter.
 *   5. Schedule note-off after length_pct * step_period µs via one-shot timer.
 */

#include "sequencer.h"
#include "midi.h"

#include <string.h>
#include <stdatomic.h>

#include "esp_log.h"
#include "esp_attr.h"       /* IRAM_ATTR                    */
#include "driver/gptimer.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sequencer";

/* ── Internal state ─────────────────────────────────────────────────────── */

/** Double buffer for pattern data. */
static seq_pattern_t s_buf[2];
static volatile uint8_t      s_active_idx  = 0;  /* index of the buffer the ISR reads  */
static atomic_bool           s_swap_pending = false;

/** Current playback state. */
static volatile uint8_t  s_current_step = 0;
static volatile bool     s_running      = false;
static volatile uint16_t s_bpm          = CONFIG_ZEQ_SEQ_DEFAULT_BPM;

/** GPTimer handle. */
static gptimer_handle_t  s_timer = NULL;

/** One-shot esp_timer for note-off events. */
static esp_timer_handle_t s_note_off_timer = NULL;

/* Cached MIDI state for note-off: the note that is currently sounding. */
static volatile uint8_t s_active_note = SEQ_NOTE_OFF;

/* ── Helpers ────────────────────────────────────────────────────────────── */

/** Convert BPM → step period in microseconds (16th-note grid). */
static inline uint64_t bpm_to_step_us(uint16_t bpm)
{
    /* 1 bar = 4 beats = 16 sixteenth notes
     * step_period = (60 * 1e6) / bpm / 4  µs          */
    return (uint64_t)(60000000UL / bpm / 4);
}

/** Active buffer pointer (called from ISR — must stay in IRAM). */
static IRAM_ATTR inline seq_pattern_t *active_buf(void)
{
    return &s_buf[s_active_idx];
}

/* ── Note-off callback (esp_timer, runs in timer task context) ────────────── */
static void note_off_cb(void *arg)
{
    uint8_t note = s_active_note;
    if (note != SEQ_NOTE_OFF) {
        midi_note_off((uint8_t)(CONFIG_ZEQ_MIDI_DEFAULT_CHANNEL - 1), note, 0);
        s_active_note = SEQ_NOTE_OFF;
    }
}

/* ── GPTimer alarm ISR ───────────────────────────────────────────────────── */
static IRAM_ATTR bool timer_isr(gptimer_handle_t timer,
                                 const gptimer_alarm_event_data_t *edata,
                                 void *user_ctx)
{
    /* 1. Swap buffers if a new pattern is staged (only at step 0 / bar start) */
    if (s_current_step == 0 && atomic_load(&s_swap_pending)) {
        s_active_idx ^= 1;                      /* toggle 0↔1 */
        atomic_store(&s_swap_pending, false);
    }

    seq_pattern_t *pat  = active_buf();
    uint8_t        step = s_current_step;

    if (pat->steps[step].active) {
        uint8_t note = pat->steps[step].note;
        uint8_t vel  = pat->steps[step].velocity;
        uint8_t chan = (uint8_t)(CONFIG_ZEQ_MIDI_DEFAULT_CHANNEL - 1);

        /* Send note-on via ISR-safe MIDI function */
        midi_note_on_from_isr(chan, note, vel);

        /* Schedule note-off proportional to step length */
        uint64_t step_us  = bpm_to_step_us(s_bpm);
        uint64_t off_delay = (step_us * pat->steps[step].length_pct) / 100UL;
        s_active_note = note;
        esp_timer_start_once(s_note_off_timer, (int64_t)off_delay);
    }

    /* 2. Advance step counter */
    s_current_step = (step + 1) % pat->num_steps;

    /* Returning true wakes a higher-priority task if needed by FreeRTOS */
    return false;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

esp_err_t sequencer_init(void)
{
    ESP_LOGI(TAG, "Initialising sequencer engine");

    /* ── Populate default pattern ── */
    seq_pattern_t *def = &s_buf[0];
    memset(def, 0, sizeof(*def));
    def->num_steps = CONFIG_ZEQ_SEQ_NUM_STEPS;
    def->bpm       = CONFIG_ZEQ_SEQ_DEFAULT_BPM;

    for (int i = 0; i < def->num_steps; i++) {
        def->steps[i].note       = CONFIG_ZEQ_SEQ_DEFAULT_NOTE;
        def->steps[i].velocity   = 100;
        def->steps[i].length_pct = 50;
        /* Default: only beat 1, 5, 9, 13 active (quarter-note pulse) */
        def->steps[i].active = (i % 4 == 0);
    }
    /* Staging buffer starts as a copy */
    memcpy(&s_buf[1], &s_buf[0], sizeof(seq_pattern_t));

    /* ── Create note-off one-shot timer ── */
    esp_timer_create_args_t note_off_args = {
        .callback        = note_off_cb,
        .arg             = NULL,
        .name            = "note_off",
        .dispatch_method = ESP_TIMER_TASK,
    };
    ESP_ERROR_CHECK(esp_timer_create(&note_off_args, &s_note_off_timer));

    /* ── Configure GPTimer ── */
    gptimer_config_t timer_cfg = {
        .clk_src       = GPTIMER_CLK_SRC_DEFAULT,
        .direction     = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, /* 1 MHz → 1 µs resolution */
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_cfg, &s_timer));

    gptimer_alarm_config_t alarm_cfg = {
        .alarm_count            = bpm_to_step_us(s_bpm),
        .reload_count           = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(s_timer, &alarm_cfg));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_isr,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(s_timer, &cbs, NULL));
    ESP_ERROR_CHECK(gptimer_enable(s_timer));

    ESP_LOGI(TAG, "Sequencer ready — %d steps @ %d BPM",
             def->num_steps, def->bpm);
    return ESP_OK;
}

void sequencer_start(void)
{
    s_current_step = 0;
    s_running      = true;
    ESP_LOGI(TAG, "Sequencer started");
    ESP_ERROR_CHECK(gptimer_start(s_timer));
}

void sequencer_stop(void)
{
    s_running = false;
    ESP_ERROR_CHECK(gptimer_stop(s_timer));

    /* Cancel any pending note-off and silence the current note */
    esp_timer_stop(s_note_off_timer);
    if (s_active_note != SEQ_NOTE_OFF) {
        midi_note_off((uint8_t)(CONFIG_ZEQ_MIDI_DEFAULT_CHANNEL - 1),
                      s_active_note, 0);
        s_active_note = SEQ_NOTE_OFF;
    }
    /* All-Notes-Off on the MIDI channel */
    midi_all_notes_off((uint8_t)(CONFIG_ZEQ_MIDI_DEFAULT_CHANNEL - 1));
    ESP_LOGI(TAG, "Sequencer stopped");
}

void sequencer_set_bpm(uint16_t bpm)
{
    if (bpm < 20 || bpm > 300) {
        ESP_LOGW(TAG, "BPM %d out of range, ignoring", bpm);
        return;
    }
    s_bpm = bpm;
    gptimer_alarm_config_t alarm_cfg = {
        .alarm_count                = bpm_to_step_us(bpm),
        .reload_count               = 0,
        .flags.auto_reload_on_alarm = true,
    };
    /* Safe to reconfigure while timer runs; new period takes effect next alarm */
    ESP_ERROR_CHECK(gptimer_set_alarm_action(s_timer, &alarm_cfg));
    ESP_LOGI(TAG, "BPM set to %d (step period: %llu µs)", bpm, bpm_to_step_us(bpm));
}

void sequencer_set_pattern(const seq_pattern_t *pattern)
{
    /* Write into the staging buffer (the one NOT currently active) */
    uint8_t staging_idx = s_active_idx ^ 1;
    memcpy(&s_buf[staging_idx], pattern, sizeof(seq_pattern_t));
    /* Signal ISR to swap at next bar boundary */
    atomic_store(&s_swap_pending, true);
    ESP_LOGD(TAG, "New pattern staged — will swap at next bar");
}

uint8_t sequencer_get_current_step(void)
{
    return s_current_step;
}

/* ── Control callback struct ─────────────────────────────────────────────── */
static const seq_control_callbacks_t s_callbacks = {
    .set_bpm         = sequencer_set_bpm,
    .set_pattern     = sequencer_set_pattern,
    .start           = sequencer_start,
    .stop            = sequencer_stop,
    .get_current_step = sequencer_get_current_step,
};

const seq_control_callbacks_t *sequencer_get_control_callbacks(void)
{
    return &s_callbacks;
}