/* components/midi/midi.c */

#include "midi.h"

#include <string.h>
#include "esp_log.h"
#include "esp_attr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "midi";

/* ── Configuration (from Kconfig) ────────────────────────────────────────── */
#define MIDI_UART_NUM   ((uart_port_t)CONFIG_ZEQ_MIDI_UART_PORT)
#define MIDI_TX_PIN     CONFIG_ZEQ_MIDI_TX_GPIO
#define MIDI_BAUD_RATE  31250
#define MIDI_QUEUE_DEPTH 64   /* Max events outstanding before oldest is dropped */

/* ── MIDI message container ──────────────────────────────────────────────── */
typedef struct {
    uint8_t data[3]; /* Up to 3 bytes; most MIDI messages are 3 bytes */
    uint8_t len;
} midi_msg_t;

/* ── Internal state ─────────────────────────────────────────────────────── */
static QueueHandle_t s_midi_queue = NULL;

/* ── Sender task ─────────────────────────────────────────────────────────── */
static void midi_sender_task(void *arg)
{
    midi_msg_t msg;
    for (;;) {
        if (xQueueReceive(s_midi_queue, &msg, portMAX_DELAY) == pdTRUE) {
            uart_write_bytes(MIDI_UART_NUM, (const char *)msg.data, msg.len);
        }
    }
}

/* ── Internal helper: enqueue a message ──────────────────────────────────── */
static inline void enqueue_msg(const midi_msg_t *msg)
{
    /* Non-blocking; if queue is full, drop the message gracefully */
    xQueueSendToBack(s_midi_queue, msg, 0);
}

static IRAM_ATTR inline void enqueue_msg_from_isr(const midi_msg_t *msg)
{
    BaseType_t higher_prio_woken = pdFALSE;
    xQueueSendToBackFromISR(s_midi_queue, msg, &higher_prio_woken);
    if (higher_prio_woken) {
        portYIELD_FROM_ISR();
    }
}

/* ── Public API ─────────────────────────────────────────────────────────── */

esp_err_t midi_init(void)
{
    ESP_LOGI(TAG, "Initialising MIDI on UART%d, TX GPIO%d @ %d baud",
             MIDI_UART_NUM, MIDI_TX_PIN, MIDI_BAUD_RATE);

    uart_config_t uart_cfg = {
        .baud_rate  = MIDI_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(MIDI_UART_NUM,
                                        256 /* rx buf */, 256 /* tx buf */,
                                        0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(MIDI_UART_NUM, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(MIDI_UART_NUM,
                                 MIDI_TX_PIN,      /* TX */
                                 UART_PIN_NO_CHANGE, /* RX — not used */
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));

    /* Message queue */
    s_midi_queue = xQueueCreate(MIDI_QUEUE_DEPTH, sizeof(midi_msg_t));
    if (!s_midi_queue) {
        ESP_LOGE(TAG, "Failed to create MIDI queue");
        return ESP_ERR_NO_MEM;
    }

    /* High-priority sender task — keeps UART fed without blocking the ISR */
    xTaskCreate(midi_sender_task, "midi_sender", 2048, NULL,
                configMAX_PRIORITIES - 2, NULL);

    ESP_LOGI(TAG, "MIDI ready");
    return ESP_OK;
}

void midi_note_on(uint8_t channel, uint8_t note, uint8_t velocity)
{
    midi_msg_t msg = {
        .data = { (uint8_t)(MIDI_NOTE_ON | (channel & 0x0F)), note & 0x7F,
                  velocity & 0x7F },
        .len  = 3,
    };
    enqueue_msg(&msg);
}

void IRAM_ATTR midi_note_on_from_isr(uint8_t channel, uint8_t note,
                                      uint8_t velocity)
{
    midi_msg_t msg = {
        .data = { (uint8_t)(MIDI_NOTE_ON | (channel & 0x0F)), note & 0x7F,
                  velocity & 0x7F },
        .len  = 3,
    };
    enqueue_msg_from_isr(&msg);
}

void midi_note_off(uint8_t channel, uint8_t note, uint8_t velocity)
{
    midi_msg_t msg = {
        .data = { (uint8_t)(MIDI_NOTE_OFF | (channel & 0x0F)), note & 0x7F,
                  velocity & 0x7F },
        .len  = 3,
    };
    enqueue_msg(&msg);
}

void midi_all_notes_off(uint8_t channel)
{
    /* CC 123, value 0 = All Notes Off */
    midi_msg_t msg = {
        .data = { (uint8_t)(MIDI_CC | (channel & 0x0F)),
                  MIDI_ALL_NOTES_OFF, 0x00 },
        .len  = 3,
    };
    enqueue_msg(&msg);
}