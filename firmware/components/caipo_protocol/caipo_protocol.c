/* GENERATED FILE - DO NOT EDIT.
 * Produced by protocol/codegen/generate.py from protocol/caipo.protocol.yaml.
 * Edit the YAML and re-run codegen, never this file directly.
 */
#include "caipo_protocol.h"

#include <string.h>

static void caipo__put_u8(uint8_t **cursor, uint8_t value) {
    **cursor = value;
    *cursor += 1;
}

static uint8_t caipo__get_u8(const uint8_t **cursor) {
    uint8_t value = **cursor;
    *cursor += 1;
    return value;
}

static void caipo__put_u16le(uint8_t **cursor, uint16_t value) {
    (*cursor)[0] = (uint8_t)(value & 0xFFu);
    (*cursor)[1] = (uint8_t)((value >> 8) & 0xFFu);
    *cursor += 2;
}

static uint16_t caipo__get_u16le(const uint8_t **cursor) {
    uint16_t value = (uint16_t)((*cursor)[0] | ((uint16_t)(*cursor)[1] << 8));
    *cursor += 2;
    return value;
}

static void caipo__put_u32le(uint8_t **cursor, uint32_t value) {
    (*cursor)[0] = (uint8_t)(value & 0xFFu);
    (*cursor)[1] = (uint8_t)((value >> 8) & 0xFFu);
    (*cursor)[2] = (uint8_t)((value >> 16) & 0xFFu);
    (*cursor)[3] = (uint8_t)((value >> 24) & 0xFFu);
    *cursor += 4;
}

static uint32_t caipo__get_u32le(const uint8_t **cursor) {
    uint32_t value = (uint32_t)(*cursor)[0]
        | ((uint32_t)(*cursor)[1] << 8)
        | ((uint32_t)(*cursor)[2] << 16)
        | ((uint32_t)(*cursor)[3] << 24);
    *cursor += 4;
    return value;
}

bool caipo_protocol_split_envelope(const uint8_t *data, size_t len, caipo_msg_type_t *msg_type, uint8_t *seq, const uint8_t **payload, size_t *payload_len) {
    if (len < 2) {
        return false;
    }
    *msg_type = (caipo_msg_type_t)data[0];
    *seq = data[1];
    *payload = data + 2;
    *payload_len = len - 2;
    return true;
}

size_t caipo_protocol_encode_heartbeat(uint8_t seq, const caipo_heartbeat_msg_t *msg, uint8_t *out, size_t out_cap) {
    size_t needed = 6u;
    if (out_cap < needed) {
        return 0;
    }

    uint8_t *cursor = out;
    caipo__put_u8(&cursor, (uint8_t)CAIPO_MSG_HEARTBEAT);
    caipo__put_u8(&cursor, seq);
    caipo__put_u32le(&cursor, (uint32_t)msg->uptime_ms);
    return (size_t)(cursor - out);
}

bool caipo_protocol_decode_heartbeat(const uint8_t *payload, size_t payload_len, caipo_heartbeat_msg_t *out) {
    if (payload_len < 4u) {
        return false;
    }
    const uint8_t *cursor = payload;
    out->uptime_ms = (uint32_t)caipo__get_u32le(&cursor);
    return true;
}

size_t caipo_protocol_encode_pi_ready(uint8_t seq, const caipo_pi_ready_msg_t *msg, uint8_t *out, size_t out_cap) {
    size_t needed = 5u;
    if (out_cap < needed) {
        return 0;
    }

    uint8_t *cursor = out;
    caipo__put_u8(&cursor, (uint8_t)CAIPO_MSG_PI_READY);
    caipo__put_u8(&cursor, seq);
    caipo__put_u8(&cursor, (uint8_t)msg->fw_major);
    caipo__put_u8(&cursor, (uint8_t)msg->fw_minor);
    caipo__put_u8(&cursor, (uint8_t)msg->fw_patch);
    return (size_t)(cursor - out);
}

bool caipo_protocol_decode_pi_ready(const uint8_t *payload, size_t payload_len, caipo_pi_ready_msg_t *out) {
    if (payload_len < 3u) {
        return false;
    }
    const uint8_t *cursor = payload;
    out->fw_major = (uint8_t)caipo__get_u8(&cursor);
    out->fw_minor = (uint8_t)caipo__get_u8(&cursor);
    out->fw_patch = (uint8_t)caipo__get_u8(&cursor);
    return true;
}

size_t caipo_protocol_encode_start_workflow(uint8_t seq, const caipo_start_workflow_msg_t *msg, uint8_t *out, size_t out_cap) {
    size_t needed = 7u;
    if (out_cap < needed) {
        return 0;
    }

    uint8_t *cursor = out;
    caipo__put_u8(&cursor, (uint8_t)CAIPO_MSG_START_WORKFLOW);
    caipo__put_u8(&cursor, seq);
    caipo__put_u8(&cursor, (uint8_t)msg->workflow);
    caipo__put_u16le(&cursor, (uint16_t)msg->duration_s);
    caipo__put_u16le(&cursor, (uint16_t)msg->scheduled_delay_s);
    return (size_t)(cursor - out);
}

bool caipo_protocol_decode_start_workflow(const uint8_t *payload, size_t payload_len, caipo_start_workflow_msg_t *out) {
    if (payload_len < 5u) {
        return false;
    }
    const uint8_t *cursor = payload;
    out->workflow = (caipo_workflow_type_t)caipo__get_u8(&cursor);
    out->duration_s = (uint16_t)caipo__get_u16le(&cursor);
    out->scheduled_delay_s = (uint16_t)caipo__get_u16le(&cursor);
    return true;
}

size_t caipo_protocol_encode_stop_workflow(uint8_t seq, uint8_t *out, size_t out_cap) {
    size_t needed = 2u;
    if (out_cap < needed) {
        return 0;
    }

    uint8_t *cursor = out;
    caipo__put_u8(&cursor, (uint8_t)CAIPO_MSG_STOP_WORKFLOW);
    caipo__put_u8(&cursor, seq);
    return (size_t)(cursor - out);
}

bool caipo_protocol_decode_stop_workflow(const uint8_t *payload, size_t payload_len) {
    (void)payload;
    return payload_len == 0;
}

size_t caipo_protocol_encode_ack(uint8_t seq, const caipo_ack_msg_t *msg, uint8_t *out, size_t out_cap) {
    size_t needed = 3u;
    if (out_cap < needed) {
        return 0;
    }

    uint8_t *cursor = out;
    caipo__put_u8(&cursor, (uint8_t)CAIPO_MSG_ACK);
    caipo__put_u8(&cursor, seq);
    caipo__put_u8(&cursor, (uint8_t)msg->ack_seq);
    return (size_t)(cursor - out);
}

bool caipo_protocol_decode_ack(const uint8_t *payload, size_t payload_len, caipo_ack_msg_t *out) {
    if (payload_len < 1u) {
        return false;
    }
    const uint8_t *cursor = payload;
    out->ack_seq = (uint8_t)caipo__get_u8(&cursor);
    return true;
}

size_t caipo_protocol_encode_complete(uint8_t seq, const caipo_complete_msg_t *msg, uint8_t *out, size_t out_cap) {
    size_t needed = 8u;
    if (out_cap < needed) {
        return 0;
    }

    uint8_t *cursor = out;
    caipo__put_u8(&cursor, (uint8_t)CAIPO_MSG_COMPLETE);
    caipo__put_u8(&cursor, seq);
    caipo__put_u8(&cursor, (uint8_t)msg->ack_seq);
    caipo__put_u8(&cursor, (uint8_t)msg->file_count);
    caipo__put_u32le(&cursor, (uint32_t)msg->bytes_captured);
    return (size_t)(cursor - out);
}

bool caipo_protocol_decode_complete(const uint8_t *payload, size_t payload_len, caipo_complete_msg_t *out) {
    if (payload_len < 6u) {
        return false;
    }
    const uint8_t *cursor = payload;
    out->ack_seq = (uint8_t)caipo__get_u8(&cursor);
    out->file_count = (uint8_t)caipo__get_u8(&cursor);
    out->bytes_captured = (uint32_t)caipo__get_u32le(&cursor);
    return true;
}

size_t caipo_protocol_encode_error(uint8_t seq, const caipo_error_msg_t *msg, uint8_t *out, size_t out_cap) {
    size_t needed = 5u + msg->message_len;
    if (msg->message_len > 64) {
        return 0;
    }
    if (out_cap < needed) {
        return 0;
    }

    uint8_t *cursor = out;
    caipo__put_u8(&cursor, (uint8_t)CAIPO_MSG_ERROR);
    caipo__put_u8(&cursor, seq);
    caipo__put_u8(&cursor, (uint8_t)msg->ack_seq);
    caipo__put_u8(&cursor, (uint8_t)msg->code);
    caipo__put_u8(&cursor, msg->message_len);
    memcpy(cursor, msg->message, msg->message_len);
    cursor += msg->message_len;
    return (size_t)(cursor - out);
}

bool caipo_protocol_decode_error(const uint8_t *payload, size_t payload_len, caipo_error_msg_t *out) {
    if (payload_len < 3u) {
        return false;
    }
    const uint8_t *cursor = payload;
    out->ack_seq = (uint8_t)caipo__get_u8(&cursor);
    out->code = (caipo_error_code_t)caipo__get_u8(&cursor);
    uint8_t message_len = caipo__get_u8(&cursor);
    if (message_len > 64) {
        return false;
    }
    if ((size_t)(cursor - payload) + message_len > payload_len) {
        return false;
    }
    memcpy(out->message, cursor, message_len);
    out->message[message_len] = '\0';
    out->message_len = message_len;
    cursor += message_len;
    return true;
}

size_t caipo_protocol_encode_battery_status(uint8_t seq, const caipo_battery_status_msg_t *msg, uint8_t *out, size_t out_cap) {
    size_t needed = 6u;
    if (out_cap < needed) {
        return 0;
    }

    uint8_t *cursor = out;
    caipo__put_u8(&cursor, (uint8_t)CAIPO_MSG_BATTERY_STATUS);
    caipo__put_u8(&cursor, seq);
    caipo__put_u8(&cursor, (uint8_t)msg->percent);
    caipo__put_u16le(&cursor, (uint16_t)msg->voltage_mv);
    caipo__put_u8(&cursor, (uint8_t)msg->charging);
    return (size_t)(cursor - out);
}

bool caipo_protocol_decode_battery_status(const uint8_t *payload, size_t payload_len, caipo_battery_status_msg_t *out) {
    if (payload_len < 4u) {
        return false;
    }
    const uint8_t *cursor = payload;
    out->percent = (uint8_t)caipo__get_u8(&cursor);
    out->voltage_mv = (uint16_t)caipo__get_u16le(&cursor);
    out->charging = caipo__get_u8(&cursor) != 0;
    return true;
}

size_t caipo_protocol_encode_button_event(uint8_t seq, const caipo_button_event_msg_t *msg, uint8_t *out, size_t out_cap) {
    size_t needed = 3u;
    if (out_cap < needed) {
        return 0;
    }

    uint8_t *cursor = out;
    caipo__put_u8(&cursor, (uint8_t)CAIPO_MSG_BUTTON_EVENT);
    caipo__put_u8(&cursor, seq);
    caipo__put_u8(&cursor, (uint8_t)msg->event);
    return (size_t)(cursor - out);
}

bool caipo_protocol_decode_button_event(const uint8_t *payload, size_t payload_len, caipo_button_event_msg_t *out) {
    if (payload_len < 1u) {
        return false;
    }
    const uint8_t *cursor = payload;
    out->event = (caipo_button_event_t)caipo__get_u8(&cursor);
    return true;
}

size_t caipo_protocol_encode_shutdown_request(uint8_t seq, const caipo_shutdown_request_msg_t *msg, uint8_t *out, size_t out_cap) {
    size_t needed = 3u;
    if (out_cap < needed) {
        return 0;
    }

    uint8_t *cursor = out;
    caipo__put_u8(&cursor, (uint8_t)CAIPO_MSG_SHUTDOWN_REQUEST);
    caipo__put_u8(&cursor, seq);
    caipo__put_u8(&cursor, (uint8_t)msg->reason);
    return (size_t)(cursor - out);
}

bool caipo_protocol_decode_shutdown_request(const uint8_t *payload, size_t payload_len, caipo_shutdown_request_msg_t *out) {
    if (payload_len < 1u) {
        return false;
    }
    const uint8_t *cursor = payload;
    out->reason = (caipo_shutdown_reason_t)caipo__get_u8(&cursor);
    return true;
}

size_t caipo_protocol_encode_shutdown_ready(uint8_t seq, uint8_t *out, size_t out_cap) {
    size_t needed = 2u;
    if (out_cap < needed) {
        return 0;
    }

    uint8_t *cursor = out;
    caipo__put_u8(&cursor, (uint8_t)CAIPO_MSG_SHUTDOWN_READY);
    caipo__put_u8(&cursor, seq);
    return (size_t)(cursor - out);
}

bool caipo_protocol_decode_shutdown_ready(const uint8_t *payload, size_t payload_len) {
    (void)payload;
    return payload_len == 0;
}

