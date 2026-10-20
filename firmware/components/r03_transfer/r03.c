#include "r03.h"

#include <string.h>

/* ---------------------------------------------------------------- utils */

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, (uint16_t)v); put16(p + 2, (uint16_t)(v >> 16)); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)get16(p) | ((uint32_t)get16(p + 2) << 16); }

uint32_t r03_crc32(uint32_t crc, const uint8_t *data, size_t len)
{
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

uint16_t r03_chunk_payload_for_mtu(uint16_t mtu)
{
    if (mtu <= 3 + R03_CHUNK_HDR_LEN) {
        return 0;
    }
    uint16_t p = (uint16_t)(mtu - 3 - R03_CHUNK_HDR_LEN);
    return p > R03_MAX_PAYLOAD ? R03_MAX_PAYLOAD : p;
}

/* ---------------------------------------------------------------- codec */

int r03_decode(const uint8_t *buf, size_t len, r03_msg_t *out)
{
    if (buf == NULL || out == NULL || len < R03_HANDSHAKE_LEN) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->op = buf[0];
    out->id = get16(buf + 1);

    switch (out->op) {
    case R03_OP_TRANSFER_REQUEST:
    case R03_OP_TRANSFER_READY:
    case R03_OP_TRANSFER_ACK:
        return len == R03_HANDSHAKE_LEN ? 0 : -1;

    case R03_OP_METADATA:
        if (len != R03_METADATA_LEN) {
            return -1;
        }
        out->total_size = get32(buf + 3);
        out->content_type = buf[7];
        out->crc32 = get32(buf + 8);
        out->chunk_count = get16(buf + 12);
        return 0;

    case R03_OP_CHUNK:
        if (len < R03_CHUNK_HDR_LEN) {
            return -1;
        }
        out->seq = get16(buf + 3);
        out->payload_len = get16(buf + 5);
        if (out->payload_len > R03_MAX_PAYLOAD || (size_t)out->payload_len + R03_CHUNK_HDR_LEN != len) {
            return -1;
        }
        out->payload = buf + R03_CHUNK_HDR_LEN;
        return 0;

    case R03_OP_TRANSFER_ERROR:
        if (len != R03_ERROR_LEN) {
            return -1;
        }
        out->error_code = buf[3];
        out->failed_seq = get16(buf + 4);
        return 0;

    default:
        return -1;
    }
}

size_t r03_encode_handshake(uint8_t op, uint16_t id, uint8_t out[R03_HANDSHAKE_LEN])
{
    out[0] = op;
    put16(out + 1, id);
    return R03_HANDSHAKE_LEN;
}

size_t r03_encode_metadata(uint16_t id, uint32_t total_size, uint8_t content_type,
                           uint32_t crc32, uint16_t chunk_count, uint8_t out[R03_METADATA_LEN])
{
    out[0] = R03_OP_METADATA;
    put16(out + 1, id);
    put32(out + 3, total_size);
    out[7] = content_type;
    put32(out + 8, crc32);
    put16(out + 12, chunk_count);
    return R03_METADATA_LEN;
}

size_t r03_encode_error(uint16_t id, uint8_t code, uint16_t failed_seq, uint8_t out[R03_ERROR_LEN])
{
    out[0] = R03_OP_TRANSFER_ERROR;
    put16(out + 1, id);
    out[3] = code;
    put16(out + 4, failed_seq);
    return R03_ERROR_LEN;
}

size_t r03_encode_chunk(uint16_t id, uint16_t seq, const uint8_t *payload, uint16_t payload_len,
                        uint8_t *out, size_t cap)
{
    if (payload_len > R03_MAX_PAYLOAD || cap < (size_t)R03_CHUNK_HDR_LEN + payload_len) {
        return 0;
    }
    out[0] = R03_OP_CHUNK;
    put16(out + 1, id);
    put16(out + 3, seq);
    put16(out + 5, payload_len);
    if (payload_len) {
        memcpy(out + R03_CHUNK_HDR_LEN, payload, payload_len);
    }
    return (size_t)R03_CHUNK_HDR_LEN + payload_len;
}

/* ------------------------------------------------------------- receiver */

static void rx_error(r03_rx_t *rx, uint16_t id, uint8_t code, uint16_t seq, r03_rx_result_t *res, bool reset)
{
    res->out_len = r03_encode_error(id, code, seq, res->out);
    if (reset) {
        r03_rx_reset(rx);
    }
}

void r03_rx_init(r03_rx_t *rx, uint8_t *buf, uint32_t cap)
{
    memset(rx, 0, sizeof(*rx));
    rx->buf = buf;
    rx->cap = cap;
    rx->state = R03_RX_IDLE;
}

void r03_rx_reset(r03_rx_t *rx)
{
    uint8_t *buf = rx->buf;
    uint32_t cap = rx->cap;
    r03_rx_init(rx, buf, cap);
}

bool r03_rx_busy(const r03_rx_t *rx)
{
    return rx->state != R03_RX_IDLE;
}

static void rx_finish(r03_rx_t *rx, r03_rx_result_t *res)
{
    if (rx->received != rx->total_size) {
        /* Last chunk arrived but bytes are missing: the sender's chunking
         * disagrees with its METADATA. Not recoverable by resend. */
        rx_error(rx, rx->id, R03_ERR_SEQUENCE_GAP, rx->next_seq, res, true);
        return;
    }
    uint32_t crc = r03_crc32(0, rx->buf, rx->received);
    if (crc != rx->crc32) {
        rx_error(rx, rx->id, R03_ERR_CHECKSUM_MISMATCH, R03_SEQ_NONE, res, true);
        return;
    }
    rx->state = R03_RX_COMPLETE;
    res->out_len = r03_encode_handshake(R03_OP_TRANSFER_ACK, rx->id, res->out);
    res->complete = true;
}

void r03_rx_feed(r03_rx_t *rx, const r03_msg_t *msg, uint32_t now_ms, r03_rx_result_t *res)
{
    memset(res, 0, sizeof(*res));

    switch (msg->op) {
    case R03_OP_TRANSFER_REQUEST:
        if (rx->state == R03_RX_COMPLETE || (rx->state != R03_RX_IDLE && rx->id != msg->id)) {
            /* A finished transfer must be consumed (r03_rx_reset) first. */
            rx_error(rx, msg->id, R03_ERR_BUSY, R03_SEQ_NONE, res, false);
            return;
        }
        /* New transfer, or the peer restarting the same one: start over. */
        r03_rx_reset(rx);
        rx->state = R03_RX_WAIT_METADATA;
        rx->id = msg->id;
        rx->last_activity_ms = now_ms;
        res->out_len = r03_encode_handshake(R03_OP_TRANSFER_READY, msg->id, res->out);
        return;

    case R03_OP_METADATA:
        if (rx->state != R03_RX_WAIT_METADATA || rx->id != msg->id) {
            rx_error(rx, msg->id, R03_ERR_UNKNOWN, R03_SEQ_NONE, res, false);
            return;
        }
        if (msg->content_type < R03_TYPE_AUDIO || msg->content_type > R03_TYPE_PROVISIONING) {
            rx_error(rx, msg->id, R03_ERR_UNSUPPORTED_TYPE, R03_SEQ_NONE, res, true);
            return;
        }
        if (msg->total_size > rx->cap || msg->total_size > R03_PROTOCOL_MAX_SIZE) {
            rx_error(rx, msg->id, R03_ERR_TOO_LARGE, R03_SEQ_NONE, res, true);
            return;
        }
        if (msg->total_size == 0 || msg->chunk_count == 0) {
            /* Nothing to transfer is not a transfer. */
            rx_error(rx, msg->id, R03_ERR_UNKNOWN, R03_SEQ_NONE, res, true);
            return;
        }
        rx->total_size = msg->total_size;
        rx->content_type = msg->content_type;
        rx->crc32 = msg->crc32;
        rx->chunk_count = msg->chunk_count;
        rx->next_seq = 0;
        rx->received = 0;
        rx->state = R03_RX_RECEIVING;
        rx->last_activity_ms = now_ms;
        return;

    case R03_OP_CHUNK:
        if (rx->state != R03_RX_RECEIVING || rx->id != msg->id) {
            rx_error(rx, msg->id, R03_ERR_UNKNOWN, R03_SEQ_NONE, res, false);
            return;
        }
        rx->last_activity_ms = now_ms;
        if (msg->seq < rx->next_seq) {
            return; /* duplicate of an already stored chunk (peer resend): ignore */
        }
        if (msg->seq > rx->next_seq || msg->seq >= rx->chunk_count) {
            /* Ask for the chunk we actually need; the peer resends from there. */
            rx_error(rx, rx->id, R03_ERR_SEQUENCE_GAP, rx->next_seq, res, false);
            return;
        }
        if (rx->received + msg->payload_len > rx->total_size) {
            rx_error(rx, rx->id, R03_ERR_TOO_LARGE, msg->seq, res, true);
            return;
        }
        memcpy(rx->buf + rx->received, msg->payload, msg->payload_len);
        rx->received += msg->payload_len;
        rx->next_seq++;
        if (msg->seq == rx->chunk_count - 1) {
            rx_finish(rx, res);
        }
        return;

    case R03_OP_TRANSFER_ERROR:
        /* Peer aborted or reported a problem with what we sent back: drop the
         * in-flight transfer so it can be restarted. */
        if (rx->state != R03_RX_IDLE && rx->id == msg->id) {
            r03_rx_reset(rx);
        }
        return;

    default:
        /* READY / ACK are sender-side messages; receiving them here means the
         * peer is confused about the direction. */
        rx_error(rx, msg->id, R03_ERR_UNKNOWN, R03_SEQ_NONE, res, false);
        return;
    }
}

void r03_rx_tick(r03_rx_t *rx, uint32_t now_ms, r03_rx_result_t *res)
{
    memset(res, 0, sizeof(*res));
    if (rx->state == R03_RX_WAIT_METADATA || rx->state == R03_RX_RECEIVING) {
        if ((uint32_t)(now_ms - rx->last_activity_ms) >= R03_TIMEOUT_MS) {
            rx_error(rx, rx->id, R03_ERR_TIMEOUT,
                     rx->state == R03_RX_RECEIVING ? rx->next_seq : R03_SEQ_NONE, res, true);
        }
    }
}

/* --------------------------------------------------------------- sender */

int r03_tx_init(r03_tx_t *tx, uint16_t id, uint8_t content_type, const uint8_t *data,
                uint32_t len, uint16_t chunk_payload, uint32_t now_ms)
{
    memset(tx, 0, sizeof(*tx));
    if (data == NULL || len == 0 || len > R03_PROTOCOL_MAX_SIZE || chunk_payload == 0 ||
        chunk_payload > R03_MAX_PAYLOAD) {
        tx->state = R03_TX_FAILED;
        tx->last_error = R03_ERR_UNKNOWN;
        return -1;
    }
    uint32_t chunks = (len + chunk_payload - 1) / chunk_payload;
    if (chunks > 0xFFFF) {
        tx->state = R03_TX_FAILED;
        tx->last_error = R03_ERR_TOO_LARGE;
        return -1;
    }
    tx->state = R03_TX_IDLE;
    tx->id = id;
    tx->content_type = content_type;
    tx->data = data;
    tx->len = len;
    tx->crc32 = r03_crc32(0, data, len);
    tx->chunk_payload = chunk_payload;
    tx->chunk_count = (uint16_t)chunks;
    tx->last_activity_ms = now_ms;
    return 0;
}

size_t r03_tx_next(r03_tx_t *tx, uint32_t now_ms, uint8_t *out, size_t cap)
{
    switch (tx->state) {
    case R03_TX_IDLE:
        if (cap < R03_HANDSHAKE_LEN) {
            return 0;
        }
        tx->state = R03_TX_WAIT_READY;
        tx->request_sent = true;
        tx->last_activity_ms = now_ms;
        return r03_encode_handshake(R03_OP_TRANSFER_REQUEST, tx->id, out);

    case R03_TX_SENDING:
        if (!tx->metadata_sent) {
            if (cap < R03_METADATA_LEN) {
                return 0;
            }
            tx->metadata_sent = true;
            return r03_encode_metadata(tx->id, tx->len, tx->content_type, tx->crc32, tx->chunk_count, out);
        }
        if (tx->next_seq < tx->chunk_count) {
            uint32_t off = (uint32_t)tx->next_seq * tx->chunk_payload;
            uint32_t remaining = tx->len - off;
            uint16_t n = remaining < tx->chunk_payload ? (uint16_t)remaining : tx->chunk_payload;
            size_t w = r03_encode_chunk(tx->id, tx->next_seq, tx->data + off, n, out, cap);
            if (w == 0) {
                return 0; /* caller's buffer too small; try again with a bigger one */
            }
            tx->next_seq++;
            if (tx->next_seq == tx->chunk_count) {
                tx->state = R03_TX_WAIT_ACK;
                tx->last_activity_ms = now_ms;
            }
            return w;
        }
        tx->state = R03_TX_WAIT_ACK;
        return 0;

    default:
        return 0;
    }
}

void r03_tx_feed(r03_tx_t *tx, const r03_msg_t *msg, uint32_t now_ms)
{
    if (msg->id != tx->id || !r03_tx_active(tx)) {
        return;
    }
    tx->last_activity_ms = now_ms;

    switch (msg->op) {
    case R03_OP_TRANSFER_READY:
        if (tx->state == R03_TX_WAIT_READY) {
            tx->state = R03_TX_SENDING;
        }
        return;

    case R03_OP_TRANSFER_ACK:
        if (tx->state == R03_TX_WAIT_ACK || tx->state == R03_TX_SENDING) {
            tx->state = R03_TX_DONE;
        }
        return;

    case R03_OP_TRANSFER_ERROR:
        if (msg->error_code == R03_ERR_SEQUENCE_GAP || msg->error_code == R03_ERR_CHECKSUM_MISMATCH) {
            if (tx->resends >= R03_MAX_RESENDS) {
                tx->state = R03_TX_FAILED;
                tx->last_error = msg->error_code;
                return;
            }
            tx->resends++;
            if (msg->error_code == R03_ERR_CHECKSUM_MISMATCH || msg->failed_seq == R03_SEQ_NONE ||
                msg->failed_seq >= tx->chunk_count) {
                /* Whole payload failed verification: the receiver dropped it,
                 * so the transfer restarts from the handshake. */
                tx->state = R03_TX_IDLE;
                tx->request_sent = false;
                tx->metadata_sent = false;
                tx->next_seq = 0;
            } else {
                /* Resume from the chunk the receiver is missing. */
                tx->next_seq = msg->failed_seq;
                tx->state = R03_TX_SENDING;
            }
            return;
        }
        tx->state = R03_TX_FAILED;
        tx->last_error = msg->error_code;
        return;

    default:
        return;
    }
}

void r03_tx_tick(r03_tx_t *tx, uint32_t now_ms)
{
    if (tx->state == R03_TX_WAIT_READY || tx->state == R03_TX_WAIT_ACK) {
        if ((uint32_t)(now_ms - tx->last_activity_ms) >= R03_TIMEOUT_MS) {
            tx->state = R03_TX_FAILED;
            tx->last_error = R03_ERR_TIMEOUT;
        }
    }
}

bool r03_tx_active(const r03_tx_t *tx)
{
    return tx->state == R03_TX_IDLE || tx->state == R03_TX_WAIT_READY ||
           tx->state == R03_TX_SENDING || tx->state == R03_TX_WAIT_ACK;
}
