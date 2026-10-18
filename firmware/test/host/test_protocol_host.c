/* Host test: runs caipo_protocol's C codec against the golden vectors in
 * /protocol/vectors/, the same fixtures software/tests/test_protocol.py runs
 * against the Python codec. A pass on both sides means the two generated
 * codecs agree byte-for-byte, not just that each is internally consistent.
 *
 * Builds standalone (plain CMake, no ESP-IDF) per firmware/README.md.
 *
 * Includes a JSON reader scoped to the fixed shape of the vector files
 * (an array of flat objects: "description", "seq", "fields", "hex") --
 * not a general-purpose parser.
 */
#include "caipo_protocol.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CAIPO_VECTORS_DIR
#define CAIPO_VECTORS_DIR "../../../protocol/vectors"
#endif

/* ---- minimal JSON reader, scoped to the vectors' fixed shape ---- */

typedef enum { FV_STRING, FV_NUMBER, FV_BOOL } field_value_type_t;

typedef struct {
    char key[32];
    field_value_type_t type;
    union {
        char string[80];
        long long number;
        bool boolean;
    } as;
} field_kv_t;

#define MAX_FIELDS 8
#define MAX_HEX_BYTES 96
#define MAX_CASES 16

typedef struct {
    char description[128];
    int seq;
    field_kv_t fields[MAX_FIELDS];
    size_t field_count;
    uint8_t hex[MAX_HEX_BYTES];
    size_t hex_len;
} vector_case_t;

typedef struct {
    const char *p;
} cursor_t;

static void die(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(2);
}

static void skip_ws(cursor_t *c) {
    while (*c->p == ' ' || *c->p == '\n' || *c->p == '\t' || *c->p == '\r') {
        c->p++;
    }
}

static void expect(cursor_t *c, char ch) {
    skip_ws(c);
    if (*c->p != ch) {
        die("json parse error: expected '%c', got '%c'", ch, *c->p ? *c->p : '?');
    }
    c->p++;
}

static void parse_string_into(cursor_t *c, char *out, size_t out_cap) {
    skip_ws(c);
    expect(c, '"');
    size_t n = 0;
    while (*c->p != '"') {
        if (*c->p == '\0') {
            die("json parse error: unterminated string");
        }
        char ch = *c->p++;
        if (ch == '\\') {
            char esc = *c->p++;
            switch (esc) {
                case '"': ch = '"'; break;
                case '\\': ch = '\\'; break;
                case '/': ch = '/'; break;
                case 'n': ch = '\n'; break;
                case 't': ch = '\t'; break;
                default: ch = esc; break;
            }
        }
        if (n + 1 < out_cap) {
            out[n++] = ch;
        }
    }
    c->p++; /* closing quote */
    out[n] = '\0';
}

static long long parse_number(cursor_t *c) {
    skip_ws(c);
    char *end = NULL;
    long long v = strtoll(c->p, &end, 10);
    if (end == c->p) {
        die("json parse error: expected number, got '%c'", *c->p ? *c->p : '?');
    }
    c->p = end;
    return v;
}

static bool parse_bool(cursor_t *c) {
    skip_ws(c);
    if (strncmp(c->p, "true", 4) == 0) {
        c->p += 4;
        return true;
    }
    if (strncmp(c->p, "false", 5) == 0) {
        c->p += 5;
        return false;
    }
    die("json parse error: expected bool");
    return false;
}

static size_t parse_fields_object(cursor_t *c, field_kv_t *out, size_t out_cap) {
    expect(c, '{');
    size_t n = 0;
    skip_ws(c);
    if (*c->p == '}') {
        c->p++;
        return 0;
    }
    for (;;) {
        if (n >= out_cap) {
            die("json parse error: too many fields (cap %zu)", out_cap);
        }
        field_kv_t *kv = &out[n];
        parse_string_into(c, kv->key, sizeof kv->key);
        expect(c, ':');
        skip_ws(c);
        if (*c->p == '"') {
            kv->type = FV_STRING;
            parse_string_into(c, kv->as.string, sizeof kv->as.string);
        } else if (*c->p == 't' || *c->p == 'f') {
            kv->type = FV_BOOL;
            kv->as.boolean = parse_bool(c);
        } else {
            kv->type = FV_NUMBER;
            kv->as.number = parse_number(c);
        }
        n++;
        skip_ws(c);
        if (*c->p == ',') {
            c->p++;
            continue;
        }
        break;
    }
    expect(c, '}');
    return n;
}

static size_t parse_hex_bytes(const char *hex, uint8_t *out, size_t out_cap) {
    size_t len = strlen(hex);
    size_t n = 0;
    for (size_t i = 0; i + 1 < len; i += 2) {
        unsigned int byte = 0;
        if (sscanf(&hex[i], "%2x", &byte) != 1) {
            die("json parse error: bad hex digit pair at offset %zu in '%s'", i, hex);
        }
        if (n >= out_cap) {
            die("hex vector too long for buffer (cap %zu)", out_cap);
        }
        out[n++] = (uint8_t)byte;
    }
    return n;
}

static void parse_vector_object(cursor_t *c, vector_case_t *v) {
    memset(v, 0, sizeof *v);
    char hex_str[2 * MAX_HEX_BYTES + 1] = {0};
    expect(c, '{');
    for (;;) {
        char key[32];
        parse_string_into(c, key, sizeof key);
        expect(c, ':');
        skip_ws(c);
        if (strcmp(key, "description") == 0) {
            parse_string_into(c, v->description, sizeof v->description);
        } else if (strcmp(key, "seq") == 0) {
            v->seq = (int)parse_number(c);
        } else if (strcmp(key, "fields") == 0) {
            v->field_count = parse_fields_object(c, v->fields, MAX_FIELDS);
        } else if (strcmp(key, "hex") == 0) {
            parse_string_into(c, hex_str, sizeof hex_str);
        } else {
            die("json parse error: unexpected key '%s'", key);
        }
        skip_ws(c);
        if (*c->p == ',') {
            c->p++;
            continue;
        }
        break;
    }
    expect(c, '}');
    v->hex_len = parse_hex_bytes(hex_str, v->hex, sizeof v->hex);
}

static size_t parse_vectors_array(const char *text, vector_case_t *out, size_t out_cap) {
    cursor_t c = {text};
    expect(&c, '[');
    size_t n = 0;
    skip_ws(&c);
    if (*c.p == ']') {
        c.p++;
        return 0;
    }
    for (;;) {
        if (n >= out_cap) {
            die("json parse error: too many vector cases (cap %zu)", out_cap);
        }
        parse_vector_object(&c, &out[n]);
        n++;
        skip_ws(&c);
        if (*c.p == ',') {
            c.p++;
            continue;
        }
        break;
    }
    expect(&c, ']');
    return n;
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        die("cannot open '%s'", path);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        die("out of memory reading '%s'", path);
    }
    size_t read_n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[read_n] = '\0';
    return buf;
}

static size_t load_vectors(const char *name, vector_case_t *out, size_t out_cap) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s.json", CAIPO_VECTORS_DIR, name);
    char *text = read_file(path);
    size_t n = parse_vectors_array(text, out, out_cap);
    free(text);
    return n;
}

/* ---- field lookup helpers ---- */

static const field_kv_t *find_field(const vector_case_t *v, const char *key) {
    for (size_t i = 0; i < v->field_count; i++) {
        if (strcmp(v->fields[i].key, key) == 0) {
            return &v->fields[i];
        }
    }
    die("vector '%s': missing field '%s'", v->description, key);
    return NULL;
}

static long long field_int(const vector_case_t *v, const char *key) { return find_field(v, key)->as.number; }
static bool field_bool(const vector_case_t *v, const char *key) { return find_field(v, key)->as.boolean; }
static const char *field_str(const vector_case_t *v, const char *key) { return find_field(v, key)->as.string; }

/* ---- enum name -> value, mirrors caipo.protocol.yaml's enums: block ---- */

static caipo_workflow_type_t workflow_type_from_name(const char *name) {
    if (strcmp(name, "QUICK_VIDEO") == 0) return CAIPO_WORKFLOW_TYPE_QUICK_VIDEO;
    if (strcmp(name, "AUDIO_ONLY") == 0) return CAIPO_WORKFLOW_TYPE_AUDIO_ONLY;
    if (strcmp(name, "SCHEDULED_WAKE") == 0) return CAIPO_WORKFLOW_TYPE_SCHEDULED_WAKE;
    die("unknown WorkflowType '%s'", name);
    return 0;
}

static caipo_button_event_t button_event_from_name(const char *name) {
    if (strcmp(name, "TAP") == 0) return CAIPO_BUTTON_EVENT_TAP;
    if (strcmp(name, "DOUBLE_TAP") == 0) return CAIPO_BUTTON_EVENT_DOUBLE_TAP;
    if (strcmp(name, "LONG_PRESS") == 0) return CAIPO_BUTTON_EVENT_LONG_PRESS;
    die("unknown ButtonEvent '%s'", name);
    return 0;
}

static caipo_error_code_t error_code_from_name(const char *name) {
    if (strcmp(name, "UNKNOWN") == 0) return CAIPO_ERROR_CODE_UNKNOWN;
    if (strcmp(name, "BUSY") == 0) return CAIPO_ERROR_CODE_BUSY;
    if (strcmp(name, "INVALID_PARAMS") == 0) return CAIPO_ERROR_CODE_INVALID_PARAMS;
    if (strcmp(name, "HARDWARE_FAULT") == 0) return CAIPO_ERROR_CODE_HARDWARE_FAULT;
    if (strcmp(name, "TIMEOUT") == 0) return CAIPO_ERROR_CODE_TIMEOUT;
    if (strcmp(name, "STORAGE_FULL") == 0) return CAIPO_ERROR_CODE_STORAGE_FULL;
    die("unknown ErrorCode '%s'", name);
    return 0;
}

static caipo_shutdown_reason_t shutdown_reason_from_name(const char *name) {
    if (strcmp(name, "USER") == 0) return CAIPO_SHUTDOWN_REASON_USER;
    if (strcmp(name, "LOW_BATTERY") == 0) return CAIPO_SHUTDOWN_REASON_LOW_BATTERY;
    if (strcmp(name, "SCHEDULED") == 0) return CAIPO_SHUTDOWN_REASON_SCHEDULED;
    die("unknown ShutdownReason '%s'", name);
    return 0;
}

/* ---- test harness ---- */

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond, ...)                                                                                             \
    do {                                                                                                             \
        g_checks++;                                                                                                  \
        if (!(cond)) {                                                                                               \
            g_failures++;                                                                                            \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);                                                     \
            fprintf(stderr, __VA_ARGS__);                                                                            \
            fprintf(stderr, "\n");                                                                                   \
        }                                                                                                            \
    } while (0)

static void check_envelope(const vector_case_t *v, const char *msg_name, caipo_msg_type_t expect_type,
                            const uint8_t **payload_out, size_t *payload_len_out) {
    caipo_msg_type_t msg_type;
    uint8_t seq;
    const uint8_t *payload;
    size_t payload_len;
    bool ok = caipo_protocol_split_envelope(v->hex, v->hex_len, &msg_type, &seq, &payload, &payload_len);
    CHECK(ok, "%s/%s: split_envelope failed", msg_name, v->description);
    CHECK(msg_type == expect_type, "%s/%s: envelope msg_type mismatch", msg_name, v->description);
    CHECK(seq == (uint8_t)v->seq, "%s/%s: envelope seq mismatch (got %u, want %d)", msg_name, v->description, seq,
          v->seq);
    *payload_out = payload;
    *payload_len_out = payload_len;
}

static void check_encoded_bytes(const vector_case_t *v, const char *msg_name, const uint8_t *buf, size_t len) {
    CHECK(len == v->hex_len, "%s/%s: encoded length %zu != expected %zu", msg_name, v->description, len,
          v->hex_len);
    CHECK(len == v->hex_len && memcmp(buf, v->hex, v->hex_len) == 0, "%s/%s: encoded bytes != golden hex", msg_name,
          v->description);
}

static void test_heartbeat(void) {
    vector_case_t cases[MAX_CASES];
    size_t n = load_vectors("heartbeat", cases, MAX_CASES);
    for (size_t i = 0; i < n; i++) {
        const vector_case_t *v = &cases[i];
        caipo_heartbeat_msg_t msg = {.uptime_ms = (uint32_t)field_int(v, "uptime_ms")};

        uint8_t buf[32];
        size_t len = caipo_protocol_encode_heartbeat((uint8_t)v->seq, &msg, buf, sizeof buf);
        check_encoded_bytes(v, "heartbeat", buf, len);

        const uint8_t *payload;
        size_t payload_len;
        check_envelope(v, "heartbeat", CAIPO_MSG_HEARTBEAT, &payload, &payload_len);

        caipo_heartbeat_msg_t decoded;
        bool ok = caipo_protocol_decode_heartbeat(payload, payload_len, &decoded);
        CHECK(ok && decoded.uptime_ms == msg.uptime_ms, "heartbeat/%s: decode mismatch", v->description);
    }
}

static void test_pi_ready(void) {
    vector_case_t cases[MAX_CASES];
    size_t n = load_vectors("pi_ready", cases, MAX_CASES);
    for (size_t i = 0; i < n; i++) {
        const vector_case_t *v = &cases[i];
        caipo_pi_ready_msg_t msg = {
            .fw_major = (uint8_t)field_int(v, "fw_major"),
            .fw_minor = (uint8_t)field_int(v, "fw_minor"),
            .fw_patch = (uint8_t)field_int(v, "fw_patch"),
        };

        uint8_t buf[32];
        size_t len = caipo_protocol_encode_pi_ready((uint8_t)v->seq, &msg, buf, sizeof buf);
        check_encoded_bytes(v, "pi_ready", buf, len);

        const uint8_t *payload;
        size_t payload_len;
        check_envelope(v, "pi_ready", CAIPO_MSG_PI_READY, &payload, &payload_len);

        caipo_pi_ready_msg_t decoded;
        bool ok = caipo_protocol_decode_pi_ready(payload, payload_len, &decoded);
        CHECK(ok && decoded.fw_major == msg.fw_major && decoded.fw_minor == msg.fw_minor &&
                  decoded.fw_patch == msg.fw_patch,
              "pi_ready/%s: decode mismatch", v->description);
    }
}

static void test_start_workflow(void) {
    vector_case_t cases[MAX_CASES];
    size_t n = load_vectors("start_workflow", cases, MAX_CASES);
    for (size_t i = 0; i < n; i++) {
        const vector_case_t *v = &cases[i];
        caipo_start_workflow_msg_t msg = {
            .workflow = workflow_type_from_name(field_str(v, "workflow")),
            .duration_s = (uint16_t)field_int(v, "duration_s"),
            .scheduled_delay_s = (uint16_t)field_int(v, "scheduled_delay_s"),
        };

        uint8_t buf[32];
        size_t len = caipo_protocol_encode_start_workflow((uint8_t)v->seq, &msg, buf, sizeof buf);
        check_encoded_bytes(v, "start_workflow", buf, len);

        const uint8_t *payload;
        size_t payload_len;
        check_envelope(v, "start_workflow", CAIPO_MSG_START_WORKFLOW, &payload, &payload_len);

        caipo_start_workflow_msg_t decoded;
        bool ok = caipo_protocol_decode_start_workflow(payload, payload_len, &decoded);
        CHECK(ok && decoded.workflow == msg.workflow && decoded.duration_s == msg.duration_s &&
                  decoded.scheduled_delay_s == msg.scheduled_delay_s,
              "start_workflow/%s: decode mismatch", v->description);
    }
}

static void test_stop_workflow(void) {
    vector_case_t cases[MAX_CASES];
    size_t n = load_vectors("stop_workflow", cases, MAX_CASES);
    for (size_t i = 0; i < n; i++) {
        const vector_case_t *v = &cases[i];

        uint8_t buf[32];
        size_t len = caipo_protocol_encode_stop_workflow((uint8_t)v->seq, buf, sizeof buf);
        check_encoded_bytes(v, "stop_workflow", buf, len);

        const uint8_t *payload;
        size_t payload_len;
        check_envelope(v, "stop_workflow", CAIPO_MSG_STOP_WORKFLOW, &payload, &payload_len);

        bool ok = caipo_protocol_decode_stop_workflow(payload, payload_len);
        CHECK(ok, "stop_workflow/%s: decode failed", v->description);
    }
}

static void test_ack(void) {
    vector_case_t cases[MAX_CASES];
    size_t n = load_vectors("ack", cases, MAX_CASES);
    for (size_t i = 0; i < n; i++) {
        const vector_case_t *v = &cases[i];
        caipo_ack_msg_t msg = {.ack_seq = (uint8_t)field_int(v, "ack_seq")};

        uint8_t buf[32];
        size_t len = caipo_protocol_encode_ack((uint8_t)v->seq, &msg, buf, sizeof buf);
        check_encoded_bytes(v, "ack", buf, len);

        const uint8_t *payload;
        size_t payload_len;
        check_envelope(v, "ack", CAIPO_MSG_ACK, &payload, &payload_len);

        caipo_ack_msg_t decoded;
        bool ok = caipo_protocol_decode_ack(payload, payload_len, &decoded);
        CHECK(ok && decoded.ack_seq == msg.ack_seq, "ack/%s: decode mismatch", v->description);
    }
}

static void test_complete(void) {
    vector_case_t cases[MAX_CASES];
    size_t n = load_vectors("complete", cases, MAX_CASES);
    for (size_t i = 0; i < n; i++) {
        const vector_case_t *v = &cases[i];
        caipo_complete_msg_t msg = {
            .ack_seq = (uint8_t)field_int(v, "ack_seq"),
            .file_count = (uint8_t)field_int(v, "file_count"),
            .bytes_captured = (uint32_t)field_int(v, "bytes_captured"),
        };

        uint8_t buf[32];
        size_t len = caipo_protocol_encode_complete((uint8_t)v->seq, &msg, buf, sizeof buf);
        check_encoded_bytes(v, "complete", buf, len);

        const uint8_t *payload;
        size_t payload_len;
        check_envelope(v, "complete", CAIPO_MSG_COMPLETE, &payload, &payload_len);

        caipo_complete_msg_t decoded;
        bool ok = caipo_protocol_decode_complete(payload, payload_len, &decoded);
        CHECK(ok && decoded.ack_seq == msg.ack_seq && decoded.file_count == msg.file_count &&
                  decoded.bytes_captured == msg.bytes_captured,
              "complete/%s: decode mismatch", v->description);
    }
}

static void test_error(void) {
    vector_case_t cases[MAX_CASES];
    size_t n = load_vectors("error", cases, MAX_CASES);
    for (size_t i = 0; i < n; i++) {
        const vector_case_t *v = &cases[i];
        const char *message = field_str(v, "message");
        size_t message_len = strlen(message);

        caipo_error_msg_t msg = {0};
        msg.ack_seq = (uint8_t)field_int(v, "ack_seq");
        msg.code = error_code_from_name(field_str(v, "code"));
        msg.message_len = (uint8_t)message_len;
        memcpy(msg.message, message, message_len);
        msg.message[message_len] = '\0';

        uint8_t buf[128];
        size_t len = caipo_protocol_encode_error((uint8_t)v->seq, &msg, buf, sizeof buf);
        check_encoded_bytes(v, "error", buf, len);

        const uint8_t *payload;
        size_t payload_len;
        check_envelope(v, "error", CAIPO_MSG_ERROR, &payload, &payload_len);

        caipo_error_msg_t decoded;
        bool ok = caipo_protocol_decode_error(payload, payload_len, &decoded);
        CHECK(ok && decoded.ack_seq == msg.ack_seq && decoded.code == msg.code &&
                  decoded.message_len == msg.message_len && memcmp(decoded.message, msg.message, message_len) == 0,
              "error/%s: decode mismatch", v->description);
    }
}

static void test_battery_status(void) {
    vector_case_t cases[MAX_CASES];
    size_t n = load_vectors("battery_status", cases, MAX_CASES);
    for (size_t i = 0; i < n; i++) {
        const vector_case_t *v = &cases[i];
        caipo_battery_status_msg_t msg = {
            .percent = (uint8_t)field_int(v, "percent"),
            .voltage_mv = (uint16_t)field_int(v, "voltage_mv"),
            .charging = field_bool(v, "charging"),
        };

        uint8_t buf[32];
        size_t len = caipo_protocol_encode_battery_status((uint8_t)v->seq, &msg, buf, sizeof buf);
        check_encoded_bytes(v, "battery_status", buf, len);

        const uint8_t *payload;
        size_t payload_len;
        check_envelope(v, "battery_status", CAIPO_MSG_BATTERY_STATUS, &payload, &payload_len);

        caipo_battery_status_msg_t decoded;
        bool ok = caipo_protocol_decode_battery_status(payload, payload_len, &decoded);
        CHECK(ok && decoded.percent == msg.percent && decoded.voltage_mv == msg.voltage_mv &&
                  decoded.charging == msg.charging,
              "battery_status/%s: decode mismatch", v->description);
    }
}

static void test_button_event(void) {
    vector_case_t cases[MAX_CASES];
    size_t n = load_vectors("button_event", cases, MAX_CASES);
    for (size_t i = 0; i < n; i++) {
        const vector_case_t *v = &cases[i];
        caipo_button_event_msg_t msg = {.event = button_event_from_name(field_str(v, "event"))};

        uint8_t buf[32];
        size_t len = caipo_protocol_encode_button_event((uint8_t)v->seq, &msg, buf, sizeof buf);
        check_encoded_bytes(v, "button_event", buf, len);

        const uint8_t *payload;
        size_t payload_len;
        check_envelope(v, "button_event", CAIPO_MSG_BUTTON_EVENT, &payload, &payload_len);

        caipo_button_event_msg_t decoded;
        bool ok = caipo_protocol_decode_button_event(payload, payload_len, &decoded);
        CHECK(ok && decoded.event == msg.event, "button_event/%s: decode mismatch", v->description);
    }
}

static void test_shutdown_request(void) {
    vector_case_t cases[MAX_CASES];
    size_t n = load_vectors("shutdown_request", cases, MAX_CASES);
    for (size_t i = 0; i < n; i++) {
        const vector_case_t *v = &cases[i];
        caipo_shutdown_request_msg_t msg = {.reason = shutdown_reason_from_name(field_str(v, "reason"))};

        uint8_t buf[32];
        size_t len = caipo_protocol_encode_shutdown_request((uint8_t)v->seq, &msg, buf, sizeof buf);
        check_encoded_bytes(v, "shutdown_request", buf, len);

        const uint8_t *payload;
        size_t payload_len;
        check_envelope(v, "shutdown_request", CAIPO_MSG_SHUTDOWN_REQUEST, &payload, &payload_len);

        caipo_shutdown_request_msg_t decoded;
        bool ok = caipo_protocol_decode_shutdown_request(payload, payload_len, &decoded);
        CHECK(ok && decoded.reason == msg.reason, "shutdown_request/%s: decode mismatch", v->description);
    }
}

static void test_shutdown_ready(void) {
    vector_case_t cases[MAX_CASES];
    size_t n = load_vectors("shutdown_ready", cases, MAX_CASES);
    for (size_t i = 0; i < n; i++) {
        const vector_case_t *v = &cases[i];

        uint8_t buf[32];
        size_t len = caipo_protocol_encode_shutdown_ready((uint8_t)v->seq, buf, sizeof buf);
        check_encoded_bytes(v, "shutdown_ready", buf, len);

        const uint8_t *payload;
        size_t payload_len;
        check_envelope(v, "shutdown_ready", CAIPO_MSG_SHUTDOWN_READY, &payload, &payload_len);

        bool ok = caipo_protocol_decode_shutdown_ready(payload, payload_len);
        CHECK(ok, "shutdown_ready/%s: decode failed", v->description);
    }
}

/* Buffers too small to hold the envelope must fail closed, not overflow. */
static void test_encode_rejects_undersized_buffer(void) {
    caipo_heartbeat_msg_t msg = {.uptime_ms = 123456};
    uint8_t buf[8];
    size_t len = caipo_protocol_encode_heartbeat(7, &msg, buf, 3);
    CHECK(len == 0, "encode_heartbeat into a too-small buffer should return 0, got %zu", len);
}

static void test_decode_rejects_truncated_payload(void) {
    vector_case_t cases[MAX_CASES];
    size_t n = load_vectors("heartbeat", cases, MAX_CASES);
    CHECK(n > 0, "heartbeat vectors missing");
    const vector_case_t *v = &cases[0];

    const uint8_t *payload;
    size_t payload_len;
    check_envelope(v, "heartbeat", CAIPO_MSG_HEARTBEAT, &payload, &payload_len);

    caipo_heartbeat_msg_t decoded;
    bool ok = caipo_protocol_decode_heartbeat(payload, payload_len - 1, &decoded);
    CHECK(!ok, "decode_heartbeat should reject a truncated payload");
}

int main(void) {
    test_heartbeat();
    test_pi_ready();
    test_start_workflow();
    test_stop_workflow();
    test_ack();
    test_complete();
    test_error();
    test_battery_status();
    test_button_event();
    test_shutdown_request();
    test_shutdown_ready();
    test_encode_rejects_undersized_buffer();
    test_decode_rejects_truncated_payload();

    if (g_failures == 0) {
        printf("PASS: %d checks across 11 messages\n", g_checks);
        return 0;
    }
    fprintf(stderr, "%d/%d checks failed\n", g_failures, g_checks);
    return 1;
}
