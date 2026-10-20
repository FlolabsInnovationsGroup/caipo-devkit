#pragma once
/*
 * R03 transfer protocol: reliable, chunked transfer of one blob in either
 * direction over BLE, as agreed with the app team (Notion task "Build the
 * CAIPO mobile app", section "BLE contract", 15 Oct 2026).
 *
 * All multi-byte fields little-endian. Opcode 0x00 is reserved.
 *
 *   0x01 TRANSFER_REQUEST  [op][id u16]                                   3 bytes
 *   0x02 TRANSFER_READY    [op][id u16]                                   3 bytes
 *   0x03 METADATA          [op][id u16][total u32][type u8][crc32 u32][chunks u16]  14 bytes
 *   0x04 CHUNK             [op][id u16][seq u16][len u16][payload]        7 + len
 *   0x05 TRANSFER_ACK      [op][id u16]                                   3 bytes
 *   0x06 TRANSFER_ERROR    [op][id u16][code u8][failed seq u16]          6 bytes
 *
 * This file has no ESP-IDF dependency. Time is passed in by the caller so it
 * compiles and runs on the host (test/host/test_r03.c). The BLE transport
 * (ble_link) and the application glue decide where bytes go; this module only
 * encodes, decodes and runs the two state machines (receiver and sender).
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opcodes */
#define R03_OP_TRANSFER_REQUEST 0x01
#define R03_OP_TRANSFER_READY   0x02
#define R03_OP_METADATA         0x03
#define R03_OP_CHUNK            0x04
#define R03_OP_TRANSFER_ACK     0x05
#define R03_OP_TRANSFER_ERROR   0x06

/* Content types (METADATA) */
#define R03_TYPE_AUDIO        0x01
#define R03_TYPE_IMAGE        0x02
#define R03_TYPE_OTHER        0x03
#define R03_TYPE_PROVISIONING 0x04 /* PROPOSED: WiFi provisioning JSON */

/* Error codes (TRANSFER_ERROR) */
#define R03_ERR_BUSY              0x01
#define R03_ERR_CHECKSUM_MISMATCH 0x02
#define R03_ERR_SEQUENCE_GAP      0x03
#define R03_ERR_TOO_LARGE         0x04
#define R03_ERR_TIMEOUT           0x05
#define R03_ERR_UNSUPPORTED_TYPE  0x06
#define R03_ERR_UNKNOWN           0xFF

#define R03_SEQ_NONE          0xFFFF /* "failed seq" when not applicable */

#define R03_HANDSHAKE_LEN     3
#define R03_METADATA_LEN      14
#define R03_ERROR_LEN         6
#define R03_CHUNK_HDR_LEN     7
#define R03_MAX_PAYLOAD       502  /* 509-byte chunk at MTU 512 */
#define R03_MAX_MSG_LEN       (R03_CHUNK_HDR_LEN + R03_MAX_PAYLOAD)
#define R03_PROTOCOL_MAX_SIZE 32000000u /* protocol limit; the device's own limit is its buffer */
#define R03_TIMEOUT_MS        5000
#define R03_MAX_RESENDS       3

/* CRC-32 (zlib / IEEE 802.3): poly 0xEDB88320 reflected, init 0xFFFFFFFF,
 * final XOR 0xFFFFFFFF. Pass crc = 0 to start, chain to continue. */
uint32_t r03_crc32(uint32_t crc, const uint8_t *data, size_t len);

/* Chunk payload size for a negotiated ATT MTU: min(502, mtu - 3 - 7).
 * Returns 0 if the MTU is too small to carry any payload. */
uint16_t r03_chunk_payload_for_mtu(uint16_t mtu);

/* ---------------------------------------------------------------- codec */

typedef struct {
    uint8_t op;
    uint16_t id;
    /* METADATA */
    uint32_t total_size;
    uint8_t content_type;
    uint32_t crc32;
    uint16_t chunk_count;
    /* CHUNK (payload points into the caller's buffer) */
    uint16_t seq;
    uint16_t payload_len;
    const uint8_t *payload;
    /* TRANSFER_ERROR */
    uint8_t error_code;
    uint16_t failed_seq;
} r03_msg_t;

/* 0 on success, -1 if malformed (unknown opcode, wrong length). */
int r03_decode(const uint8_t *buf, size_t len, r03_msg_t *out);

size_t r03_encode_handshake(uint8_t op, uint16_t id, uint8_t out[R03_HANDSHAKE_LEN]);
size_t r03_encode_metadata(uint16_t id, uint32_t total_size, uint8_t content_type,
                           uint32_t crc32, uint16_t chunk_count, uint8_t out[R03_METADATA_LEN]);
size_t r03_encode_error(uint16_t id, uint8_t code, uint16_t failed_seq, uint8_t out[R03_ERROR_LEN]);
/* Returns 0 if cap < 7 + payload_len or payload_len > R03_MAX_PAYLOAD. */
size_t r03_encode_chunk(uint16_t id, uint16_t seq, const uint8_t *payload, uint16_t payload_len,
                        uint8_t *out, size_t cap);

/* ------------------------------------------------------------- receiver */

typedef enum {
    R03_RX_IDLE,
    R03_RX_WAIT_METADATA, /* READY sent, waiting for METADATA */
    R03_RX_RECEIVING,
    R03_RX_COMPLETE,      /* payload verified; call r03_rx_reset() to free the slot */
} r03_rx_state_t;

typedef struct {
    r03_rx_state_t state;
    uint8_t *buf;        /* caller-owned reassembly buffer */
    uint32_t cap;        /* its size: the device's real max transfer size */
    uint16_t id;
    uint32_t total_size;
    uint8_t content_type;
    uint32_t crc32;
    uint16_t chunk_count;
    uint16_t next_seq;
    uint32_t received;
    uint32_t last_activity_ms;
} r03_rx_t;

/* What the receiver wants done after a feed/tick: bytes to send back to the
 * peer (if out_len > 0), and whether a transfer just completed. */
typedef struct {
    uint8_t out[R03_METADATA_LEN];
    size_t out_len;
    bool complete;
} r03_rx_result_t;

void r03_rx_init(r03_rx_t *rx, uint8_t *buf, uint32_t cap);
void r03_rx_reset(r03_rx_t *rx);
bool r03_rx_busy(const r03_rx_t *rx);
/* Feed one decoded message that arrived on the inbound characteristic. */
void r03_rx_feed(r03_rx_t *rx, const r03_msg_t *msg, uint32_t now_ms, r03_rx_result_t *res);
/* Call periodically: emits TRANSFER_ERROR TIMEOUT and resets after 5 s of silence. */
void r03_rx_tick(r03_rx_t *rx, uint32_t now_ms, r03_rx_result_t *res);

/* --------------------------------------------------------------- sender */

typedef enum {
    R03_TX_IDLE,
    R03_TX_WAIT_READY,
    R03_TX_SENDING,   /* METADATA then CHUNKs come out of r03_tx_next() */
    R03_TX_WAIT_ACK,
    R03_TX_DONE,
    R03_TX_FAILED,
} r03_tx_state_t;

typedef struct {
    r03_tx_state_t state;
    uint16_t id;
    uint8_t content_type;
    const uint8_t *data; /* caller-owned, must stay valid until DONE/FAILED */
    uint32_t len;
    uint32_t crc32;
    uint16_t chunk_payload;
    uint16_t chunk_count;
    uint16_t next_seq;
    bool metadata_sent;
    bool request_sent;
    uint8_t resends;
    uint8_t last_error;  /* R03_ERR_* when state == FAILED */
    uint32_t last_activity_ms;
} r03_tx_t;

/* -1 if len == 0, len > R03_PROTOCOL_MAX_SIZE, chunk_payload == 0, or the
 * chunk count would not fit in 16 bits. */
int r03_tx_init(r03_tx_t *tx, uint16_t id, uint8_t content_type, const uint8_t *data,
                uint32_t len, uint16_t chunk_payload, uint32_t now_ms);
/* Next message to send, or 0 if there is nothing to send right now (waiting
 * for the peer, done, or failed). Call repeatedly while it returns > 0. */
size_t r03_tx_next(r03_tx_t *tx, uint32_t now_ms, uint8_t *out, size_t cap);
/* Feed a decoded message from the peer (READY, ACK, ERROR). */
void r03_tx_feed(r03_tx_t *tx, const r03_msg_t *msg, uint32_t now_ms);
/* Timeout check: FAILED with R03_ERR_TIMEOUT after 5 s without peer activity
 * while waiting for READY or ACK. */
void r03_tx_tick(r03_tx_t *tx, uint32_t now_ms);
bool r03_tx_active(const r03_tx_t *tx);

#ifdef __cplusplus
}
#endif
