/* Host test for the R03 transfer codec and state machines (no ESP-IDF).
 *
 * Build standalone, e.g.
 *   cc -std=c11 -Wall -Wextra -I../../components/r03_transfer/include \
 *      test_r03.c ../../components/r03_transfer/r03.c -o test_r03 && ./test_r03
 * or through test/host/CMakeLists.txt.
 */
#include "r03.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks, g_failures;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        g_checks++;                                                                    \
        if (!(cond)) {                                                                 \
            g_failures++;                                                              \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);            \
        }                                                                              \
    } while (0)

static void fill(uint8_t *buf, size_t n, uint32_t seed)
{
    for (size_t i = 0; i < n; i++) {
        seed = seed * 1103515245u + 12345u;
        buf[i] = (uint8_t)(seed >> 16);
    }
}

/* ------------------------------------------------------------ crc / mtu */

static void test_crc32(void)
{
    const uint8_t s[] = "123456789";
    CHECK(r03_crc32(0, s, 9) == 0xCBF43926u);
    /* chaining gives the same result as one call */
    uint32_t c = r03_crc32(0, s, 4);
    c = r03_crc32(c, s + 4, 5);
    CHECK(c == 0xCBF43926u);
    CHECK(r03_crc32(0, NULL, 0) == 0);
}

static void test_chunk_payload(void)
{
    CHECK(r03_chunk_payload_for_mtu(512) == 502);
    CHECK(r03_chunk_payload_for_mtu(517) == 502);
    CHECK(r03_chunk_payload_for_mtu(185) == 175); /* typical iPhone */
    CHECK(r03_chunk_payload_for_mtu(23) == 13);   /* BLE default MTU */
    CHECK(r03_chunk_payload_for_mtu(10) == 0);
}

/* ---------------------------------------------------------------- codec */

static void test_codec_roundtrip(void)
{
    uint8_t buf[R03_MAX_MSG_LEN];
    r03_msg_t m;

    CHECK(r03_encode_handshake(R03_OP_TRANSFER_REQUEST, 0x1234, buf) == 3);
    CHECK(buf[0] == 0x01 && buf[1] == 0x34 && buf[2] == 0x12); /* little-endian */
    CHECK(r03_decode(buf, 3, &m) == 0 && m.op == R03_OP_TRANSFER_REQUEST && m.id == 0x1234);

    CHECK(r03_encode_metadata(7, 0x01020304, R03_TYPE_AUDIO, 0xDEADBEEF, 0x0203, buf) == 14);
    CHECK(r03_decode(buf, 14, &m) == 0);
    CHECK(m.op == R03_OP_METADATA && m.id == 7 && m.total_size == 0x01020304);
    CHECK(m.content_type == R03_TYPE_AUDIO && m.crc32 == 0xDEADBEEF && m.chunk_count == 0x0203);
    CHECK(buf[3] == 0x04 && buf[6] == 0x01 && buf[8] == 0xEF && buf[12] == 0x03);

    uint8_t payload[502];
    fill(payload, sizeof(payload), 1);
    size_t n = r03_encode_chunk(9, 0xABCD, payload, 502, buf, sizeof(buf));
    CHECK(n == 509);
    CHECK(r03_decode(buf, n, &m) == 0);
    CHECK(m.op == R03_OP_CHUNK && m.id == 9 && m.seq == 0xABCD && m.payload_len == 502);
    CHECK(memcmp(m.payload, payload, 502) == 0);

    CHECK(r03_encode_error(3, R03_ERR_SEQUENCE_GAP, 0x0102, buf) == 6);
    CHECK(r03_decode(buf, 6, &m) == 0);
    CHECK(m.op == R03_OP_TRANSFER_ERROR && m.id == 3 && m.error_code == R03_ERR_SEQUENCE_GAP && m.failed_seq == 0x0102);
}

static void test_codec_rejects(void)
{
    uint8_t buf[R03_MAX_MSG_LEN] = { 0 };
    r03_msg_t m;

    CHECK(r03_decode(buf, 3, &m) == -1); /* opcode 0x00 reserved */
    buf[0] = R03_OP_TRANSFER_READY;
    CHECK(r03_decode(buf, 2, &m) == -1); /* too short */
    CHECK(r03_decode(buf, 4, &m) == -1); /* too long */
    buf[0] = R03_OP_METADATA;
    CHECK(r03_decode(buf, 13, &m) == -1);
    buf[0] = R03_OP_CHUNK;
    buf[5] = 10; buf[6] = 0;             /* says 10 bytes of payload */
    CHECK(r03_decode(buf, 7 + 9, &m) == -1);
    CHECK(r03_decode(buf, 7 + 10, &m) == 0);
    buf[5] = 0xF7; buf[6] = 0x01;        /* 503 > max payload */
    CHECK(r03_decode(buf, 7 + 503, &m) == -1);
    buf[0] = 0x07;
    CHECK(r03_decode(buf, 3, &m) == -1); /* unknown opcode */
    CHECK(r03_encode_chunk(1, 0, buf, 503, buf, sizeof(buf)) == 0);
    CHECK(r03_encode_chunk(1, 0, buf, 10, buf, 16) == 0); /* cap too small */
}

/* ------------------------------------------------------------- receiver */

/* Drive a receiver through a complete app -> device transfer. */
static void app_sends(r03_rx_t *rx, uint16_t id, uint8_t type, const uint8_t *data, uint32_t len,
                      uint16_t payload, uint32_t t0, bool *acked)
{
    uint8_t buf[R03_MAX_MSG_LEN];
    r03_msg_t m;
    r03_rx_result_t res;
    *acked = false;

    r03_encode_handshake(R03_OP_TRANSFER_REQUEST, id, buf);
    CHECK(r03_decode(buf, 3, &m) == 0);
    r03_rx_feed(rx, &m, t0, &res);
    CHECK(res.out_len == 3 && res.out[0] == R03_OP_TRANSFER_READY);
    CHECK(rx->state == R03_RX_WAIT_METADATA);

    uint16_t chunks = (uint16_t)((len + payload - 1) / payload);
    r03_encode_metadata(id, len, type, r03_crc32(0, data, len), chunks, buf);
    CHECK(r03_decode(buf, 14, &m) == 0);
    r03_rx_feed(rx, &m, t0 + 10, &res);
    if (rx->state != R03_RX_RECEIVING) {
        return; /* caller checks the error */
    }
    CHECK(res.out_len == 0);

    for (uint16_t seq = 0; seq < chunks; seq++) {
        uint32_t off = (uint32_t)seq * payload;
        uint16_t n = (uint16_t)(len - off < payload ? len - off : payload);
        size_t w = r03_encode_chunk(id, seq, data + off, n, buf, sizeof(buf));
        CHECK(r03_decode(buf, w, &m) == 0);
        r03_rx_feed(rx, &m, t0 + 20 + seq, &res);
        if (seq + 1 < chunks) {
            CHECK(res.out_len == 0 && !res.complete);
        }
    }
    if (res.complete) {
        CHECK(res.out_len == 3 && res.out[0] == R03_OP_TRANSFER_ACK);
        *acked = true;
    }
}

static void test_rx_happy_path(void)
{
    static uint8_t store[4096], data[1300];
    r03_rx_t rx;
    bool acked;
    fill(data, sizeof(data), 42);
    r03_rx_init(&rx, store, sizeof(store));

    app_sends(&rx, 0x0101, R03_TYPE_AUDIO, data, sizeof(data), 175, 1000, &acked);
    CHECK(acked);
    CHECK(rx.state == R03_RX_COMPLETE);
    CHECK(rx.received == 1300 && rx.content_type == R03_TYPE_AUDIO);
    CHECK(memcmp(store, data, 1300) == 0);

    /* While complete and not consumed, a new request is refused as BUSY. */
    uint8_t buf[3];
    r03_msg_t m;
    r03_rx_result_t res;
    r03_encode_handshake(R03_OP_TRANSFER_REQUEST, 0x0202, buf);
    r03_decode(buf, 3, &m);
    r03_rx_feed(&rx, &m, 2000, &res);
    CHECK(res.out_len == 6 && res.out[0] == R03_OP_TRANSFER_ERROR && res.out[3] == R03_ERR_BUSY);

    r03_rx_reset(&rx);
    CHECK(!r03_rx_busy(&rx));

    /* A one-chunk transfer at full MTU. */
    app_sends(&rx, 0x0303, R03_TYPE_PROVISIONING, data, 502, 502, 3000, &acked);
    CHECK(acked && rx.received == 502);
    r03_rx_reset(&rx);

    /* Exactly a multiple of the payload. */
    app_sends(&rx, 0x0404, R03_TYPE_IMAGE, data, 350, 175, 4000, &acked);
    CHECK(acked && rx.chunk_count == 2);
}

static void test_rx_sequence_gap(void)
{
    static uint8_t store[2048], data[1000];
    uint8_t buf[R03_MAX_MSG_LEN];
    r03_msg_t m;
    r03_rx_result_t res;
    r03_rx_t rx;
    fill(data, sizeof(data), 7);
    r03_rx_init(&rx, store, sizeof(store));

    r03_encode_handshake(R03_OP_TRANSFER_REQUEST, 5, buf);
    r03_decode(buf, 3, &m);
    r03_rx_feed(&rx, &m, 0, &res);
    r03_encode_metadata(5, 1000, R03_TYPE_OTHER, r03_crc32(0, data, 1000), 4, buf);
    r03_decode(buf, 14, &m);
    r03_rx_feed(&rx, &m, 1, &res);

    /* chunk 0 ok, chunk 2 arrives instead of 1 -> SEQUENCE_GAP failed seq 1 */
    size_t w = r03_encode_chunk(5, 0, data, 250, buf, sizeof(buf));
    r03_decode(buf, w, &m);
    r03_rx_feed(&rx, &m, 2, &res);
    CHECK(res.out_len == 0);
    w = r03_encode_chunk(5, 2, data + 500, 250, buf, sizeof(buf));
    r03_decode(buf, w, &m);
    r03_rx_feed(&rx, &m, 3, &res);
    CHECK(res.out_len == 6 && res.out[3] == R03_ERR_SEQUENCE_GAP && res.out[4] == 1 && res.out[5] == 0);
    CHECK(rx.state == R03_RX_RECEIVING);

    /* sender resends 1, 2, 3 -> complete */
    for (uint16_t seq = 1; seq < 4; seq++) {
        w = r03_encode_chunk(5, seq, data + seq * 250, 250, buf, sizeof(buf));
        r03_decode(buf, w, &m);
        r03_rx_feed(&rx, &m, 10 + seq, &res);
    }
    CHECK(res.complete && res.out[0] == R03_OP_TRANSFER_ACK);
    CHECK(memcmp(store, data, 1000) == 0);

    /* a duplicate of an old chunk after completion is just an UNKNOWN-state error, not a crash */
    w = r03_encode_chunk(5, 1, data + 250, 250, buf, sizeof(buf));
    r03_decode(buf, w, &m);
    r03_rx_feed(&rx, &m, 20, &res);
    CHECK(res.out_len == 6 && res.out[3] == R03_ERR_UNKNOWN);
}

static void test_rx_errors(void)
{
    static uint8_t store[1024], data[2000];
    uint8_t buf[R03_MAX_MSG_LEN];
    r03_msg_t m;
    r03_rx_result_t res;
    r03_rx_t rx;
    bool acked;
    fill(data, sizeof(data), 9);
    r03_rx_init(&rx, store, sizeof(store));

    /* TOO_LARGE: 2000 bytes into a 1024 buffer */
    app_sends(&rx, 1, R03_TYPE_AUDIO, data, 2000, 175, 0, &acked);
    CHECK(!acked && rx.state == R03_RX_IDLE);

    /* UNSUPPORTED_TYPE */
    r03_encode_handshake(R03_OP_TRANSFER_REQUEST, 2, buf);
    r03_decode(buf, 3, &m);
    r03_rx_feed(&rx, &m, 0, &res);
    r03_encode_metadata(2, 100, 0x09, 0, 1, buf);
    r03_decode(buf, 14, &m);
    r03_rx_feed(&rx, &m, 1, &res);
    CHECK(res.out_len == 6 && res.out[3] == R03_ERR_UNSUPPORTED_TYPE && rx.state == R03_RX_IDLE);

    /* CHECKSUM_MISMATCH: metadata CRC does not match the bytes */
    r03_encode_handshake(R03_OP_TRANSFER_REQUEST, 3, buf);
    r03_decode(buf, 3, &m);
    r03_rx_feed(&rx, &m, 0, &res);
    r03_encode_metadata(3, 100, R03_TYPE_AUDIO, 0x12345678, 1, buf);
    r03_decode(buf, 14, &m);
    r03_rx_feed(&rx, &m, 1, &res);
    size_t w = r03_encode_chunk(3, 0, data, 100, buf, sizeof(buf));
    r03_decode(buf, w, &m);
    r03_rx_feed(&rx, &m, 2, &res);
    CHECK(res.out_len == 6 && res.out[3] == R03_ERR_CHECKSUM_MISMATCH && !res.complete);
    CHECK(rx.state == R03_RX_IDLE);

    /* METADATA without a request */
    r03_encode_metadata(4, 100, R03_TYPE_AUDIO, 0, 1, buf);
    r03_decode(buf, 14, &m);
    r03_rx_feed(&rx, &m, 3, &res);
    CHECK(res.out_len == 6 && res.out[3] == R03_ERR_UNKNOWN);

    /* TIMEOUT: request, then silence for 5 s */
    r03_encode_handshake(R03_OP_TRANSFER_REQUEST, 6, buf);
    r03_decode(buf, 3, &m);
    r03_rx_feed(&rx, &m, 10000, &res);
    r03_rx_tick(&rx, 14999, &res);
    CHECK(res.out_len == 0 && rx.state == R03_RX_WAIT_METADATA);
    r03_rx_tick(&rx, 15000, &res);
    CHECK(res.out_len == 6 && res.out[3] == R03_ERR_TIMEOUT && rx.state == R03_RX_IDLE);

    /* BUSY: a different id while one is in flight; the same id restarts */
    r03_encode_handshake(R03_OP_TRANSFER_REQUEST, 7, buf);
    r03_decode(buf, 3, &m);
    r03_rx_feed(&rx, &m, 0, &res);
    r03_encode_handshake(R03_OP_TRANSFER_REQUEST, 8, buf);
    r03_decode(buf, 3, &m);
    r03_rx_feed(&rx, &m, 1, &res);
    CHECK(res.out_len == 6 && res.out[3] == R03_ERR_BUSY && rx.id == 7);
    r03_encode_handshake(R03_OP_TRANSFER_REQUEST, 7, buf);
    r03_decode(buf, 3, &m);
    r03_rx_feed(&rx, &m, 2, &res);
    CHECK(res.out_len == 3 && res.out[0] == R03_OP_TRANSFER_READY);

    /* peer aborts with TRANSFER_ERROR -> slot freed */
    r03_encode_error(7, R03_ERR_UNKNOWN, R03_SEQ_NONE, buf);
    r03_decode(buf, 6, &m);
    r03_rx_feed(&rx, &m, 3, &res);
    CHECK(rx.state == R03_RX_IDLE && res.out_len == 0);
}

/* --------------------------------------------------------------- sender */

/* Loopback: a device sender talking to a receiver standing in for the app. */
static void test_tx_loopback(void)
{
    static uint8_t data[3000], store[4096];
    uint8_t wire[R03_MAX_MSG_LEN];
    r03_msg_t m;
    r03_rx_result_t res;
    r03_tx_t tx;
    r03_rx_t rx;
    fill(data, sizeof(data), 99);
    r03_rx_init(&rx, store, sizeof(store));

    CHECK(r03_tx_init(&tx, 0x8001, R03_TYPE_AUDIO, data, sizeof(data), 175, 0) == 0);
    CHECK(tx.chunk_count == 18); /* ceil(3000/175) */
    CHECK(r03_tx_active(&tx));

    int messages = 0;
    uint32_t t = 1;
    while (r03_tx_active(&tx) && messages < 100) {
        size_t n = r03_tx_next(&tx, t, wire, sizeof(wire));
        if (n == 0) {
            break;
        }
        messages++;
        CHECK(n <= 7 + 175);
        CHECK(r03_decode(wire, n, &m) == 0);
        r03_rx_feed(&rx, &m, t, &res);
        if (res.out_len) {
            CHECK(r03_decode(res.out, res.out_len, &m) == 0);
            r03_tx_feed(&tx, &m, t);
        }
        t++;
    }
    CHECK(messages == 1 + 1 + 18); /* REQUEST, METADATA, 18 chunks */
    CHECK(tx.state == R03_TX_DONE);
    CHECK(rx.state == R03_RX_COMPLETE && rx.received == 3000);
    CHECK(memcmp(store, data, 3000) == 0);
    CHECK(!r03_tx_active(&tx));
}

static void test_tx_resend_and_failures(void)
{
    static uint8_t data[1000];
    uint8_t wire[R03_MAX_MSG_LEN], buf[8];
    r03_msg_t m;
    r03_tx_t tx;
    fill(data, sizeof(data), 5);

    CHECK(r03_tx_init(&tx, 1, R03_TYPE_OTHER, data, 1000, 250, 0) == 0);
    CHECK(tx.chunk_count == 4);

    /* REQUEST goes out, state WAIT_READY, nothing more until READY */
    CHECK(r03_tx_next(&tx, 0, wire, sizeof(wire)) == 3 && wire[0] == R03_OP_TRANSFER_REQUEST);
    CHECK(tx.state == R03_TX_WAIT_READY);
    CHECK(r03_tx_next(&tx, 1, wire, sizeof(wire)) == 0);

    /* a READY for another id is ignored */
    r03_encode_handshake(R03_OP_TRANSFER_READY, 2, buf);
    r03_decode(buf, 3, &m);
    r03_tx_feed(&tx, &m, 2);
    CHECK(tx.state == R03_TX_WAIT_READY);

    r03_encode_handshake(R03_OP_TRANSFER_READY, 1, buf);
    r03_decode(buf, 3, &m);
    r03_tx_feed(&tx, &m, 2);
    CHECK(tx.state == R03_TX_SENDING);

    CHECK(r03_tx_next(&tx, 3, wire, sizeof(wire)) == 14 && wire[0] == R03_OP_METADATA);
    for (int i = 0; i < 4; i++) {
        size_t n = r03_tx_next(&tx, 4 + i, wire, sizeof(wire));
        CHECK(n == 257 && wire[0] == R03_OP_CHUNK && wire[3] == i);
    }
    CHECK(tx.state == R03_TX_WAIT_ACK);
    CHECK(r03_tx_next(&tx, 10, wire, sizeof(wire)) == 0);

    /* receiver missed chunk 2 -> resend from 2 */
    r03_encode_error(1, R03_ERR_SEQUENCE_GAP, 2, buf);
    r03_decode(buf, 6, &m);
    r03_tx_feed(&tx, &m, 11);
    CHECK(tx.state == R03_TX_SENDING && tx.next_seq == 2 && tx.resends == 1);
    CHECK(r03_tx_next(&tx, 12, wire, sizeof(wire)) == 257 && wire[3] == 2);
    CHECK(r03_tx_next(&tx, 13, wire, sizeof(wire)) == 257 && wire[3] == 3);
    CHECK(tx.state == R03_TX_WAIT_ACK);

    /* CHECKSUM_MISMATCH restarts from the handshake */
    r03_encode_error(1, R03_ERR_CHECKSUM_MISMATCH, R03_SEQ_NONE, buf);
    r03_decode(buf, 6, &m);
    r03_tx_feed(&tx, &m, 14);
    CHECK(tx.state == R03_TX_IDLE && tx.resends == 2);
    CHECK(r03_tx_next(&tx, 15, wire, sizeof(wire)) == 3 && wire[0] == R03_OP_TRANSFER_REQUEST);

    /* two more gap errors exhaust the 3 resends */
    r03_encode_handshake(R03_OP_TRANSFER_READY, 1, buf);
    r03_decode(buf, 3, &m);
    r03_tx_feed(&tx, &m, 16);
    r03_encode_error(1, R03_ERR_SEQUENCE_GAP, 0, buf);
    r03_decode(buf, 6, &m);
    r03_tx_feed(&tx, &m, 17);
    CHECK(tx.state == R03_TX_SENDING && tx.resends == 3);
    r03_tx_feed(&tx, &m, 18);
    CHECK(tx.state == R03_TX_FAILED && tx.last_error == R03_ERR_SEQUENCE_GAP);
    CHECK(!r03_tx_active(&tx));

    /* BUSY from the receiver fails immediately */
    CHECK(r03_tx_init(&tx, 9, R03_TYPE_OTHER, data, 10, 502, 0) == 0);
    CHECK(tx.chunk_count == 1);
    r03_tx_next(&tx, 0, wire, sizeof(wire));
    r03_encode_error(9, R03_ERR_BUSY, R03_SEQ_NONE, buf);
    r03_decode(buf, 6, &m);
    r03_tx_feed(&tx, &m, 1);
    CHECK(tx.state == R03_TX_FAILED && tx.last_error == R03_ERR_BUSY);

    /* timeout waiting for READY */
    CHECK(r03_tx_init(&tx, 10, R03_TYPE_OTHER, data, 10, 502, 100) == 0);
    r03_tx_next(&tx, 100, wire, sizeof(wire));
    r03_tx_tick(&tx, 5099);
    CHECK(tx.state == R03_TX_WAIT_READY);
    r03_tx_tick(&tx, 5100);
    CHECK(tx.state == R03_TX_FAILED && tx.last_error == R03_ERR_TIMEOUT);

    /* ACK ends the transfer */
    CHECK(r03_tx_init(&tx, 11, R03_TYPE_OTHER, data, 10, 502, 0) == 0);
    r03_tx_next(&tx, 0, wire, sizeof(wire));
    r03_encode_handshake(R03_OP_TRANSFER_READY, 11, buf);
    r03_decode(buf, 3, &m);
    r03_tx_feed(&tx, &m, 1);
    CHECK(r03_tx_next(&tx, 2, wire, sizeof(wire)) == 14);
    CHECK(r03_tx_next(&tx, 3, wire, sizeof(wire)) == 17);
    CHECK(tx.state == R03_TX_WAIT_ACK);
    r03_encode_handshake(R03_OP_TRANSFER_ACK, 11, buf);
    r03_decode(buf, 3, &m);
    r03_tx_feed(&tx, &m, 4);
    CHECK(tx.state == R03_TX_DONE);

    /* init rejects nonsense */
    CHECK(r03_tx_init(&tx, 1, R03_TYPE_OTHER, data, 0, 502, 0) == -1);
    CHECK(r03_tx_init(&tx, 1, R03_TYPE_OTHER, data, 10, 0, 0) == -1);
    CHECK(r03_tx_init(&tx, 1, R03_TYPE_OTHER, data, 10, 503, 0) == -1);
    CHECK(r03_tx_init(&tx, 1, R03_TYPE_OTHER, NULL, 10, 502, 0) == -1);
    CHECK(r03_tx_init(&tx, 1, R03_TYPE_OTHER, data, R03_PROTOCOL_MAX_SIZE + 1, 502, 0) == -1);
    /* 32 MB at 13-byte chunks (MTU 23) would need > 65535 chunks */
    CHECK(r03_tx_init(&tx, 1, R03_TYPE_OTHER, data, 1000000, 13, 0) == -1);
    CHECK(!r03_tx_active(&tx));
}

int main(void)
{
    test_crc32();
    test_chunk_payload();
    test_codec_roundtrip();
    test_codec_rejects();
    test_rx_happy_path();
    test_rx_sequence_gap();
    test_rx_errors();
    test_tx_loopback();
    test_tx_resend_and_failures();

    printf("r03: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
