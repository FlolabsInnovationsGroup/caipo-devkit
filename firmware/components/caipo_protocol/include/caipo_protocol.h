/* GENERATED FILE - DO NOT EDIT.
 * Produced by protocol/codegen/generate.py from protocol/caipo.protocol.yaml.
 * Edit the YAML and re-run codegen, never this file directly.
 */
#ifndef CAIPO_PROTOCOL_H
#define CAIPO_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CAIPO_MSG_HEARTBEAT = 1,
    CAIPO_MSG_PI_READY = 2,
    CAIPO_MSG_START_WORKFLOW = 3,
    CAIPO_MSG_STOP_WORKFLOW = 4,
    CAIPO_MSG_ACK = 5,
    CAIPO_MSG_COMPLETE = 6,
    CAIPO_MSG_ERROR = 7,
    CAIPO_MSG_BATTERY_STATUS = 8,
    CAIPO_MSG_BUTTON_EVENT = 9,
    CAIPO_MSG_SHUTDOWN_REQUEST = 10,
    CAIPO_MSG_SHUTDOWN_READY = 11,
} caipo_msg_type_t;

typedef enum {
    CAIPO_WORKFLOW_TYPE_QUICK_VIDEO = 1,
    CAIPO_WORKFLOW_TYPE_AUDIO_ONLY = 2,
    CAIPO_WORKFLOW_TYPE_SCHEDULED_WAKE = 3,
} caipo_workflow_type_t;

typedef enum {
    CAIPO_BUTTON_EVENT_TAP = 1,
    CAIPO_BUTTON_EVENT_DOUBLE_TAP = 2,
    CAIPO_BUTTON_EVENT_LONG_PRESS = 3,
} caipo_button_event_t;

typedef enum {
    CAIPO_ERROR_CODE_UNKNOWN = 0,
    CAIPO_ERROR_CODE_BUSY = 1,
    CAIPO_ERROR_CODE_INVALID_PARAMS = 2,
    CAIPO_ERROR_CODE_HARDWARE_FAULT = 3,
    CAIPO_ERROR_CODE_TIMEOUT = 4,
    CAIPO_ERROR_CODE_STORAGE_FULL = 5,
} caipo_error_code_t;

typedef enum {
    CAIPO_SHUTDOWN_REASON_USER = 0,
    CAIPO_SHUTDOWN_REASON_LOW_BATTERY = 1,
    CAIPO_SHUTDOWN_REASON_SCHEDULED = 2,
} caipo_shutdown_reason_t;

typedef struct {
    uint32_t uptime_ms;
} caipo_heartbeat_msg_t;

typedef struct {
    uint8_t fw_major;
    uint8_t fw_minor;
    uint8_t fw_patch;
} caipo_pi_ready_msg_t;

typedef struct {
    caipo_workflow_type_t workflow;
    uint16_t duration_s;
    uint16_t scheduled_delay_s;
} caipo_start_workflow_msg_t;

typedef struct {
    uint8_t ack_seq;
} caipo_ack_msg_t;

typedef struct {
    uint8_t ack_seq;
    uint8_t file_count;
    uint32_t bytes_captured;
} caipo_complete_msg_t;

typedef struct {
    uint8_t ack_seq;
    caipo_error_code_t code;
    uint8_t message_len;
    char message[64 + 1]; /* +1 for NUL, not sent on the wire */
} caipo_error_msg_t;

typedef struct {
    uint8_t percent;
    uint16_t voltage_mv;
    bool charging;
} caipo_battery_status_msg_t;

typedef struct {
    caipo_button_event_t event;
} caipo_button_event_msg_t;

typedef struct {
    caipo_shutdown_reason_t reason;
} caipo_shutdown_request_msg_t;

/* Splits a raw buffer into its envelope fields. `payload`/`payload_len` point
 * into `data` (no copy); pass them to the matching caipo_protocol_decode_* below.
 * Returns false if `data` is too short to contain an envelope. */
bool caipo_protocol_split_envelope(const uint8_t *data, size_t len, caipo_msg_type_t *msg_type, uint8_t *seq, const uint8_t **payload, size_t *payload_len);

size_t caipo_protocol_encode_heartbeat(uint8_t seq, const caipo_heartbeat_msg_t *msg, uint8_t *out, size_t out_cap);
bool caipo_protocol_decode_heartbeat(const uint8_t *payload, size_t payload_len, caipo_heartbeat_msg_t *out);

size_t caipo_protocol_encode_pi_ready(uint8_t seq, const caipo_pi_ready_msg_t *msg, uint8_t *out, size_t out_cap);
bool caipo_protocol_decode_pi_ready(const uint8_t *payload, size_t payload_len, caipo_pi_ready_msg_t *out);

size_t caipo_protocol_encode_start_workflow(uint8_t seq, const caipo_start_workflow_msg_t *msg, uint8_t *out, size_t out_cap);
bool caipo_protocol_decode_start_workflow(const uint8_t *payload, size_t payload_len, caipo_start_workflow_msg_t *out);

size_t caipo_protocol_encode_stop_workflow(uint8_t seq, uint8_t *out, size_t out_cap);
bool caipo_protocol_decode_stop_workflow(const uint8_t *payload, size_t payload_len);

size_t caipo_protocol_encode_ack(uint8_t seq, const caipo_ack_msg_t *msg, uint8_t *out, size_t out_cap);
bool caipo_protocol_decode_ack(const uint8_t *payload, size_t payload_len, caipo_ack_msg_t *out);

size_t caipo_protocol_encode_complete(uint8_t seq, const caipo_complete_msg_t *msg, uint8_t *out, size_t out_cap);
bool caipo_protocol_decode_complete(const uint8_t *payload, size_t payload_len, caipo_complete_msg_t *out);

size_t caipo_protocol_encode_error(uint8_t seq, const caipo_error_msg_t *msg, uint8_t *out, size_t out_cap);
bool caipo_protocol_decode_error(const uint8_t *payload, size_t payload_len, caipo_error_msg_t *out);

size_t caipo_protocol_encode_battery_status(uint8_t seq, const caipo_battery_status_msg_t *msg, uint8_t *out, size_t out_cap);
bool caipo_protocol_decode_battery_status(const uint8_t *payload, size_t payload_len, caipo_battery_status_msg_t *out);

size_t caipo_protocol_encode_button_event(uint8_t seq, const caipo_button_event_msg_t *msg, uint8_t *out, size_t out_cap);
bool caipo_protocol_decode_button_event(const uint8_t *payload, size_t payload_len, caipo_button_event_msg_t *out);

size_t caipo_protocol_encode_shutdown_request(uint8_t seq, const caipo_shutdown_request_msg_t *msg, uint8_t *out, size_t out_cap);
bool caipo_protocol_decode_shutdown_request(const uint8_t *payload, size_t payload_len, caipo_shutdown_request_msg_t *out);

size_t caipo_protocol_encode_shutdown_ready(uint8_t seq, uint8_t *out, size_t out_cap);
bool caipo_protocol_decode_shutdown_ready(const uint8_t *payload, size_t payload_len);

#ifdef __cplusplus
}
#endif

#endif /* CAIPO_PROTOCOL_H */
