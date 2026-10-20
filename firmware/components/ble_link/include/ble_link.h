#pragma once
/*
 * BLE peripheral for the CAIPO companion app (NimBLE host).
 *
 * GATT, as agreed with the app team:
 *   Service  A1000000-5fa6-43f5-9827-201f62bd377d
 *   A1000001 notify         device -> app data (R03)
 *   A1000002 read (enc)     audio codec ID, 0x15
 *   A1000003 write (enc)    app -> device data (R03)
 *   A1000004 write/notify   control: heartbeat, commands
 *   Battery Service 0x180F / Battery Level 0x2A19
 *
 * Security: LE Secure Connections, Just Works (no display), bonding on,
 * bonds persisted in NVS. The device initiates pairing right after a
 * connection so iOS shows its pairing dialog, and nothing is notified
 * before the link is encrypted.
 *
 * Events are delivered from the NimBLE host task: keep handlers short and
 * hand work to your own task (main.c does this with a queue).
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BLE_LINK_EVT_CONNECTED,    /* link up, not yet encrypted */
    BLE_LINK_EVT_ENCRYPTED,    /* pairing/bonding done, link encrypted: safe to talk */
    BLE_LINK_EVT_DISCONNECTED, /* data[0] = reason */
    BLE_LINK_EVT_MTU,          /* data = u16 LE MTU */
    BLE_LINK_EVT_DATA_WRITE,   /* app wrote to A1000003 (R03 message) */
    BLE_LINK_EVT_CTRL_WRITE,   /* app wrote to A1000004 (command) */
} ble_link_event_t;

typedef void (*ble_link_cb_t)(ble_link_event_t ev, const uint8_t *data, size_t len, void *ctx);

/* Start the controller, host, GATT server and advertising. NVS must be
 * initialised first (bonds live there). */
esp_err_t ble_link_init(ble_link_cb_t cb, void *ctx);

/* Notify on A1000001 (R03 data). Fails with ESP_ERR_INVALID_STATE when not
 * connected+encrypted+subscribed, ESP_ERR_INVALID_SIZE when len > MTU - 3,
 * ESP_ERR_NO_MEM when the stack is out of buffers (retry shortly). */
esp_err_t ble_link_notify_data(const uint8_t *data, size_t len);
/* Notify on A1000004 (control). Same error semantics. */
esp_err_t ble_link_notify_ctrl(const uint8_t *data, size_t len);

void ble_link_set_battery(uint8_t percent);

bool ble_link_is_connected(void);
bool ble_link_is_encrypted(void);
uint16_t ble_link_mtu(void);
/* R03 chunk payload for the current MTU: min(502, MTU - 3 - 7). */
uint16_t ble_link_chunk_payload(void);
/* The advertised name, e.g. "CAIPO3F2A". */
const char *ble_link_name(void);

/* Delete all bonds (long-press "forget phones"). Disconnects if connected. */
esp_err_t ble_link_forget_bonds(void);

#ifdef __cplusplus
}
#endif
