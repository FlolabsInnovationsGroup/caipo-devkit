/*
 * Boot and v0 application glue for the BLE link.
 *
 * What runs here today:
 *   - NVS, then the BLE peripheral (ble_link) advertising the agreed GATT.
 *   - An application task that owns the R03 state machines. BLE and touch
 *     callbacks only post events to its queue, so no protocol work happens
 *     in the NimBLE host task.
 *   - App -> device: any R03 transfer is received, verified and logged.
 *   - Device -> app: a touch TAP sends a small test blob over R03 (stands in
 *     for a recording until Opus capture lands in v1).
 *   - Control: GET_STATUS and SET_TIME answered; other commands get
 *     CMD_RESULT UNSUPPORTED.
 *   - Long press clears all bonds ("forget phones").
 *
 * This glue moves into caipo_app once the state machine component exists.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "ble_link.h"
#include "caipo_ble_proto.h"
#include "r03.h"
#include "touch_ttp223.h"

/* Antoine's I2S loopback test (mic -> speaker). Off while BLE is being
 * brought up; set to 1 to hear the microphone on the speaker. */
#define CAIPO_AUDIO_LOOPBACK_TEST 0
#if CAIPO_AUDIO_LOOPBACK_TEST
#include "mic_inmp441.h"
#include "spk_max98357.h"
#endif
QueueHandle_t audio_queue = NULL; /* shared by mic_inmp441 / spk_max98357 */

static const char *TAG = "main";

/* Device-side limit for inbound transfers. The protocol allows 32 MB; this
 * buffer is what the S3 can actually hold until a storage partition exists. */
#define RX_CAPACITY (64 * 1024)

/* Device-initiated transfers use ids from 0x8000 so they never collide with
 * ids the app picks (proposed to the app team, not yet in the contract). */
#define DEVICE_TX_ID_BASE 0x8000

typedef enum {
    EVT_BLE_CONNECTED,
    EVT_BLE_ENCRYPTED,
    EVT_BLE_DISCONNECTED,
    EVT_BLE_MTU,
    EVT_BLE_DATA,
    EVT_BLE_CTRL,
    EVT_TOUCH,
} evt_type_t;

typedef struct {
    evt_type_t type;
    uint16_t len;
    uint8_t data[R03_MAX_MSG_LEN];
} app_evt_t;

static QueueHandle_t s_events;
static uint8_t *s_rx_buf;
static r03_rx_t s_rx;
static r03_tx_t s_tx;
static bool s_tx_in_use;
static uint16_t s_next_tx_id = DEVICE_TX_ID_BASE;
static uint8_t s_pending[R03_MAX_MSG_LEN];
static size_t s_pending_len;
static uint8_t s_demo_blob[3000];
static uint32_t s_transfers_received;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* ------------------------------------------------------------ callbacks */

static void post(evt_type_t type, const uint8_t *data, size_t len)
{
    app_evt_t ev;
    ev.type = type;
    ev.len = (uint16_t)(len > sizeof(ev.data) ? sizeof(ev.data) : len);
    if (ev.len) {
        memcpy(ev.data, data, ev.len);
    }
    if (xQueueSend(s_events, &ev, 0) != pdPASS) {
        ESP_LOGW(TAG, "event queue full, dropped type %d", type);
    }
}

static void on_ble(ble_link_event_t ev, const uint8_t *data, size_t len, void *ctx)
{
    (void)ctx;
    switch (ev) {
    case BLE_LINK_EVT_CONNECTED:    post(EVT_BLE_CONNECTED, NULL, 0); break;
    case BLE_LINK_EVT_ENCRYPTED:    post(EVT_BLE_ENCRYPTED, NULL, 0); break;
    case BLE_LINK_EVT_DISCONNECTED: post(EVT_BLE_DISCONNECTED, data, len); break;
    case BLE_LINK_EVT_MTU:          post(EVT_BLE_MTU, data, len); break;
    case BLE_LINK_EVT_DATA_WRITE:   post(EVT_BLE_DATA, data, len); break;
    case BLE_LINK_EVT_CTRL_WRITE:   post(EVT_BLE_CTRL, data, len); break;
    }
}

static void on_touch(touch_event_t event, void *ctx)
{
    (void)ctx;
    uint8_t e = (uint8_t)event;
    post(EVT_TOUCH, &e, 1);
}

/* -------------------------------------------------------------- control */

static void send_ctrl(const uint8_t *msg, size_t len)
{
    esp_err_t err = ble_link_notify_ctrl(msg, len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ctrl notify 0x%02x failed: %s", msg[0], esp_err_to_name(err));
    }
}

static void send_cmd_result(uint8_t opcode, uint8_t result)
{
    uint8_t msg[3] = { CAIPO_EVT_CMD_RESULT, opcode, result };
    send_ctrl(msg, sizeof(msg));
}

static void send_status(void)
{
    uint8_t flags = 0;
    if (s_tx_in_use && r03_tx_active(&s_tx)) {
        flags |= CAIPO_FLAG_QUEUED_FOR_APP;
    }
    uint8_t msg[6] = { CAIPO_EVT_STATUS, CAIPO_STATE_STANDBY, 100 /* battery: ADC driver pending */, flags, 0, 0 };
    send_ctrl(msg, sizeof(msg));
}

static void handle_ctrl(const uint8_t *d, size_t n)
{
    if (n == 0) {
        return;
    }
    switch (d[0]) {
    case CAIPO_CMD_GET_STATUS:
        send_status();
        break;

    case CAIPO_CMD_SET_TIME: {
        if (n < 5) {
            send_cmd_result(d[0], CAIPO_RESULT_FAILED);
            break;
        }
        uint32_t t = (uint32_t)d[1] | ((uint32_t)d[2] << 8) | ((uint32_t)d[3] << 16) | ((uint32_t)d[4] << 24);
        struct timeval tv = { .tv_sec = (time_t)t, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        ESP_LOGI(TAG, "time set to %lu (unix)", (unsigned long)t);
        send_cmd_result(d[0], CAIPO_RESULT_OK);
        break;
    }

    default:
        ESP_LOGI(TAG, "command 0x%02x not implemented yet", d[0]);
        send_cmd_result(d[0], CAIPO_RESULT_UNSUPPORTED);
        break;
    }
}

/* ------------------------------------------------------------------ R03 */

static void rx_dispatch(const r03_rx_result_t *res)
{
    if (res->out_len) {
        esp_err_t err = ble_link_notify_data(res->out, res->out_len);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "R03 reply 0x%02x failed: %s", res->out[0], esp_err_to_name(err));
        }
    }
    if (res->complete) {
        s_transfers_received++;
        ESP_LOGI(TAG, "transfer %u complete: %lu bytes, type %u, CRC ok (total %lu)", s_rx.id,
                 (unsigned long)s_rx.received, s_rx.content_type, (unsigned long)s_transfers_received);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, s_rx.buf, s_rx.received < 32 ? s_rx.received : 32, ESP_LOG_INFO);
        /* v1: audio -> Opus decode -> speaker; provisioning -> NVS -> Pi. */
        r03_rx_reset(&s_rx);
    }
}

static void handle_data(const uint8_t *d, size_t n)
{
    r03_msg_t msg;
    if (r03_decode(d, n, &msg) != 0) {
        ESP_LOGW(TAG, "malformed R03 message (%u bytes, op 0x%02x)", (unsigned)n, n ? d[0] : 0);
        return;
    }
    uint32_t t = now_ms();

    /* Replies to our own outgoing transfer go to the sender. */
    if (s_tx_in_use && r03_tx_active(&s_tx) && msg.id == s_tx.id &&
        (msg.op == R03_OP_TRANSFER_READY || msg.op == R03_OP_TRANSFER_ACK || msg.op == R03_OP_TRANSFER_ERROR)) {
        r03_tx_feed(&s_tx, &msg, t);
        return;
    }

    r03_rx_result_t res;
    r03_rx_feed(&s_rx, &msg, t, &res);
    rx_dispatch(&res);
}

static void start_demo_transfer(void)
{
    if (!ble_link_is_encrypted()) {
        ESP_LOGI(TAG, "phone not connected: recording would be queued (v1)");
        return;
    }
    if (s_tx_in_use && r03_tx_active(&s_tx)) {
        ESP_LOGI(TAG, "transfer %u still in progress", s_tx.id);
        return;
    }
    uint16_t payload = ble_link_chunk_payload();
    for (size_t i = 0; i < sizeof(s_demo_blob); i++) {
        s_demo_blob[i] = (uint8_t)(i ^ (i >> 8));
    }
    snprintf((char *)s_demo_blob, 40, "CAIPO test transfer #%u", (unsigned)(s_next_tx_id - DEVICE_TX_ID_BASE));
    if (r03_tx_init(&s_tx, s_next_tx_id, R03_TYPE_OTHER, s_demo_blob, sizeof(s_demo_blob), payload, now_ms()) != 0) {
        ESP_LOGW(TAG, "tx init failed (payload %u)", payload);
        return;
    }
    s_tx_in_use = true;
    s_next_tx_id++;
    ESP_LOGI(TAG, "sending transfer %u: %u bytes in %u chunks of %u", s_tx.id, (unsigned)sizeof(s_demo_blob),
             s_tx.chunk_count, payload);
}

static void pump(void)
{
    uint32_t t = now_ms();

    r03_rx_result_t res;
    r03_rx_tick(&s_rx, t, &res);
    if (res.out_len) {
        ESP_LOGW(TAG, "inbound transfer timed out");
    }
    rx_dispatch(&res);

    if (!s_tx_in_use) {
        return;
    }
    r03_tx_tick(&s_tx, t);

    if (r03_tx_active(&s_tx) && ble_link_is_encrypted()) {
        /* A message the stack could not buffer last time goes first. */
        if (s_pending_len) {
            esp_err_t err = ble_link_notify_data(s_pending, s_pending_len);
            if (err == ESP_ERR_NO_MEM) {
                return;
            }
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "notify failed: %s", esp_err_to_name(err));
            }
            s_pending_len = 0;
        }
        for (int i = 0; i < 8; i++) {
            size_t n = r03_tx_next(&s_tx, t, s_pending, sizeof(s_pending));
            if (n == 0) {
                break;
            }
            esp_err_t err = ble_link_notify_data(s_pending, n);
            if (err == ESP_ERR_NO_MEM) {
                s_pending_len = n; /* retry on the next pass */
                break;
            }
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "notify failed: %s", esp_err_to_name(err));
                s_tx.state = R03_TX_FAILED;
                s_tx.last_error = R03_ERR_UNKNOWN;
                break;
            }
        }
    }

    if (s_tx.state == R03_TX_DONE) {
        ESP_LOGI(TAG, "transfer %u acknowledged by the app", s_tx.id);
        s_tx_in_use = false;
        s_pending_len = 0;
    } else if (s_tx.state == R03_TX_FAILED) {
        ESP_LOGW(TAG, "transfer %u failed, error 0x%02x", s_tx.id, s_tx.last_error);
        s_tx_in_use = false;
        s_pending_len = 0;
    }
}

/* ------------------------------------------------------------- app task */

static void app_task(void *param)
{
    (void)param;
    app_evt_t ev;
    for (;;) {
        if (xQueueReceive(s_events, &ev, pdMS_TO_TICKS(20)) == pdPASS) {
            switch (ev.type) {
            case EVT_BLE_CONNECTED:
                ESP_LOGI(TAG, "phone connected, waiting for encryption");
                break;
            case EVT_BLE_ENCRYPTED:
                ESP_LOGI(TAG, "link encrypted, MTU %u, chunk payload %u", ble_link_mtu(), ble_link_chunk_payload());
                break;
            case EVT_BLE_DISCONNECTED:
                ESP_LOGI(TAG, "phone disconnected (0x%02x)", ev.len ? ev.data[0] : 0);
                r03_rx_reset(&s_rx);
                if (s_tx_in_use) {
                    s_tx.state = R03_TX_FAILED;
                    s_tx.last_error = R03_ERR_TIMEOUT;
                }
                break;
            case EVT_BLE_MTU:
                break;
            case EVT_BLE_DATA:
                handle_data(ev.data, ev.len);
                break;
            case EVT_BLE_CTRL:
                handle_ctrl(ev.data, ev.len);
                break;
            case EVT_TOUCH:
                if (ev.data[0] == TOUCH_EVENT_TAP) {
                    start_demo_transfer();
                } else if (ev.data[0] == TOUCH_EVENT_LONG_PRESS) {
                    ESP_LOGI(TAG, "long press: forgetting all bonded phones");
                    ble_link_forget_bonds();
                }
                break;
            }
        }
        pump();
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    s_events = xQueueCreate(16, sizeof(app_evt_t));
    s_rx_buf = malloc(RX_CAPACITY);
    if (s_events == NULL || s_rx_buf == NULL) {
        ESP_LOGE(TAG, "out of memory at boot");
        return;
    }
    r03_rx_init(&s_rx, s_rx_buf, RX_CAPACITY);

    /* Not fatal: Wokwi has no Bluetooth controller, and a radio fault must
     * not take the touch/LED/mic path down with it. */
    err = ble_link_init(on_ble, NULL);
    if (err == ESP_OK) {
        ble_link_set_battery(100); /* placeholder until battery_adc exists */
    } else {
        ESP_LOGE(TAG, "BLE unavailable (%s); running without the app link", esp_err_to_name(err));
    }

    err = touch_init();
    if (err == ESP_OK) {
        touch_register_callback(on_touch, NULL);
    } else {
        ESP_LOGW(TAG, "touch init failed: %s", esp_err_to_name(err));
    }

    xTaskCreate(app_task, "caipo_app", 8192, NULL, 6, NULL);

#if CAIPO_AUDIO_LOOPBACK_TEST
    audio_queue = xQueueCreate(1, sizeof(uint8_t *));
    mic_main();
    spk_main();
#endif

    ESP_LOGI(TAG, "CAIPO firmware up: BLE v0 (contract of 15 Oct 2026)");
}
