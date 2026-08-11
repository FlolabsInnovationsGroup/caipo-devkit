
"""Renders firmware/components/caipo_protocol/{include/caipo_protocol.h,caipo_protocol.c}
from a validated Protocol.

The generated codec is plain C99 with no ESP-IDF dependency ("host-compilable"),
matching firmware/components/caipo_protocol/README's stated contract. It defines
message *payload* codecs only, plus a generic envelope splitter; framing (start/stop
bytes, escaping, CRC, retries) belongs to uart_link, not here.
"""

from __future__ import annotations

from schema import Field, Message, Protocol, to_lower_snake_case

_C_PRIMITIVE = {
    "u8": "uint8_t",
    "i8": "int8_t",
    "u16": "uint16_t",
    "i16": "int16_t",
    "u32": "uint32_t",
    "i32": "int32_t",
    "f32": "float",
    "bool": "bool",
}

_PRIMITIVE_SIZE = {
    "u8": 1,
    "i8": 1,
    "u16": 2,
    "i16": 2,
    "u32": 4,
    "i32": 4,
    "f32": 4,
    "bool": 1,
}

_HEADER_GUARD = "CAIPO_PROTOCOL_H"


def pascal_to_snake(name: str) -> str:
    """WorkflowType -> workflow_type"""
    out: list[str] = []
    for i, ch in enumerate(name):
        if ch.isupper() and i > 0:
            out.append("_")
        out.append(ch.lower())
    return "".join(out)


def _enum_type_name(enum_name: str) -> str:
    return f"caipo_{pascal_to_snake(enum_name)}_t"


def _enum_member_name(enum_name: str, member_name: str) -> str:
    return f"CAIPO_{pascal_to_snake(enum_name).upper()}_{member_name}"


def _payload_struct_name(message: Message) -> str:
    # "_msg_t" (not just "_t") avoids colliding with an enum of the same underlying
    # name, e.g. BUTTON_EVENT message vs. ButtonEvent enum both being "button_event".
    return f"caipo_{to_lower_snake_case(message.name)}_msg_t"


def _c_field_type(field: Field) -> str:
    if field.is_enum:
        return _enum_type_name(field.enum_name)
    return _C_PRIMITIVE[field.type]


def _field_min_size(field: Field) -> int:
    # Minimum bytes this field occupies on the wire (strings: 1-byte length prefix).
    if field.is_string:
        return 1
    if field.is_enum:
        return 1
    return _PRIMITIVE_SIZE[field.type]


def render_header(protocol: Protocol, source_path: str) -> str:
    lines: list[str] = []
    out = lines.append

    out("/* GENERATED FILE - DO NOT EDIT.")
    out(f" * Produced by protocol/codegen/generate.py from {source_path}.")
    out(" * Edit the YAML and re-run codegen, never this file directly.")
    out(" */")
    out(f"#ifndef {_HEADER_GUARD}")
    out(f"#define {_HEADER_GUARD}")
    out("")
    out("#include <stdbool.h>")
    out("#include <stddef.h>")
    out("#include <stdint.h>")
    out("")
    out("#ifdef __cplusplus")
    out('extern "C" {')
    out("#endif")
    out("")

    out("typedef enum {")
    for message in protocol.messages:
        out(f"    CAIPO_MSG_{message.name} = {message.id},")
    out(f"}} caipo_msg_type_t;")
    out("")

    for enum_def in protocol.enums.values():
        type_name = _enum_type_name(enum_def.name)
        out("typedef enum {")
        for member_name, value in enum_def.values.items():
            out(f"    {_enum_member_name(enum_def.name, member_name)} = {value},")
        out(f"}} {type_name};")
        out("")

    for message in protocol.messages:
        if not message.has_payload:
            continue
        struct_name = _payload_struct_name(message)
        out("typedef struct {")
        for field in message.fields:
            if field.is_string:
                out(f"    uint8_t {field.name}_len;")
                out(f"    char {field.name}[{field.max_length} + 1]; /* +1 for NUL, not sent on the wire */")
            else:
                out(f"    {_c_field_type(field)} {field.name};")
        out(f"}} {struct_name};")
        out("")

    out(
        "/* Splits a raw buffer into its envelope fields. `payload`/`payload_len` point\n"
        " * into `data` (no copy); pass them to the matching caipo_protocol_decode_* below.\n"
        " * Returns false if `data` is too short to contain an envelope. */"
    )
    out(
        "bool caipo_protocol_split_envelope(const uint8_t *data, size_t len, "
        "caipo_msg_type_t *msg_type, uint8_t *seq, const uint8_t **payload, size_t *payload_len);"
    )
    out("")

    for message in protocol.messages:
        lower = to_lower_snake_case(message.name)
        if message.has_payload:
            struct_name = _payload_struct_name(message)
            out(
                f"size_t caipo_protocol_encode_{lower}(uint8_t seq, const {struct_name} *msg, "
                "uint8_t *out, size_t out_cap);"
            )
            out(
                f"bool caipo_protocol_decode_{lower}(const uint8_t *payload, size_t payload_len, "
                f"{struct_name} *out);"
            )
        else:
            out(f"size_t caipo_protocol_encode_{lower}(uint8_t seq, uint8_t *out, size_t out_cap);")
            out(f"bool caipo_protocol_decode_{lower}(const uint8_t *payload, size_t payload_len);")
        out("")

    out("#ifdef __cplusplus")
    out("}")
    out("#endif")
    out("")
    out(f"#endif /* {_HEADER_GUARD} */")

    return "\n".join(lines) + "\n"


def render_source(protocol: Protocol, source_path: str) -> str:
    lines: list[str] = []
    out = lines.append

    out("/* GENERATED FILE - DO NOT EDIT.")
    out(f" * Produced by protocol/codegen/generate.py from {source_path}.")
    out(" * Edit the YAML and re-run codegen, never this file directly.")
    out(" */")
    out('#include "caipo_protocol.h"')
    out("")
    out("#include <string.h>")
    out("")
    _render_byte_helpers(out, _needed_widths(protocol))

    out(
        "bool caipo_protocol_split_envelope(const uint8_t *data, size_t len, "
        "caipo_msg_type_t *msg_type, uint8_t *seq, const uint8_t **payload, size_t *payload_len) {"
    )
    out("    if (len < 2) {")
    out("        return false;")
    out("    }")
    out("    *msg_type = (caipo_msg_type_t)data[0];")
    out("    *seq = data[1];")
    out("    *payload = data + 2;")
    out("    *payload_len = len - 2;")
    out("    return true;")
    out("}")
    out("")

    for message in protocol.messages:
        _render_message_codec(out, message)

    return "\n".join(lines) + "\n"


def _needed_widths(protocol: Protocol) -> set[str]:
    """Which byte-helper groups ("u8"/"u16"/"u32"/"f32") the schema actually uses.

    u8 is always needed (envelope msg_type/seq bytes); the rest are only emitted
    if some field actually requires them, so an unused static helper (e.g. f32
    when no message has a float field) doesn't trip -Wunused-function.
    """
    widths = {"u8"}
    for message in protocol.messages:
        for field in message.fields:
            if field.type in ("u16", "i16"):
                widths.add("u16")
            elif field.type in ("u32", "i32"):
                widths.add("u32")
            elif field.type == "f32":
                widths.add("u32")  # f32 is encoded via the u32 bit-pattern helpers
                widths.add("f32")
    return widths


def _render_byte_helpers(out, widths: set[str]) -> None:
    if "u8" in widths:
        out("static void caipo__put_u8(uint8_t **cursor, uint8_t value) {")
        out("    **cursor = value;")
        out("    *cursor += 1;")
        out("}")
        out("")
        out("static uint8_t caipo__get_u8(const uint8_t **cursor) {")
        out("    uint8_t value = **cursor;")
        out("    *cursor += 1;")
        out("    return value;")
        out("}")
        out("")

    if "u16" in widths:
        out("static void caipo__put_u16le(uint8_t **cursor, uint16_t value) {")
        out("    (*cursor)[0] = (uint8_t)(value & 0xFFu);")
        out("    (*cursor)[1] = (uint8_t)((value >> 8) & 0xFFu);")
        out("    *cursor += 2;")
        out("}")
        out("")
        out("static uint16_t caipo__get_u16le(const uint8_t **cursor) {")
        out(
            "    uint16_t value = (uint16_t)((*cursor)[0] | ((uint16_t)(*cursor)[1] << 8));"
        )
        out("    *cursor += 2;")
        out("    return value;")
        out("}")
        out("")

    if "u32" in widths:
        out("static void caipo__put_u32le(uint8_t **cursor, uint32_t value) {")
        out("    (*cursor)[0] = (uint8_t)(value & 0xFFu);")
        out("    (*cursor)[1] = (uint8_t)((value >> 8) & 0xFFu);")
        out("    (*cursor)[2] = (uint8_t)((value >> 16) & 0xFFu);")
        out("    (*cursor)[3] = (uint8_t)((value >> 24) & 0xFFu);")
        out("    *cursor += 4;")
        out("}")
        out("")
        out("static uint32_t caipo__get_u32le(const uint8_t **cursor) {")
        out("    uint32_t value = (uint32_t)(*cursor)[0]")
        out("        | ((uint32_t)(*cursor)[1] << 8)")
        out("        | ((uint32_t)(*cursor)[2] << 16)")
        out("        | ((uint32_t)(*cursor)[3] << 24);")
        out("    *cursor += 4;")
        out("    return value;")
        out("}")
        out("")

    if "f32" in widths:
        out("static void caipo__put_f32le(uint8_t **cursor, float value) {")
        out("    uint32_t bits;")
        out("    memcpy(&bits, &value, sizeof(bits));")
        out("    caipo__put_u32le(cursor, bits);")
        out("}")
        out("")
        out("static float caipo__get_f32le(const uint8_t **cursor) {")
        out("    uint32_t bits = caipo__get_u32le(cursor);")
        out("    float value;")
        out("    memcpy(&value, &bits, sizeof(value));")
        out("    return value;")
        out("}")
        out("")


def _render_message_codec(out, message: Message) -> None:
    lower = to_lower_snake_case(message.name)
    struct_name = _payload_struct_name(message) if message.has_payload else None

    _render_encode(out, message, lower, struct_name)
    out("")
    _render_decode(out, message, lower, struct_name)
    out("")


def _render_encode(out, message: Message, lower: str, struct_name: str | None) -> None:
    if struct_name is not None:
        out(
            f"size_t caipo_protocol_encode_{lower}(uint8_t seq, const {struct_name} *msg, "
            "uint8_t *out, size_t out_cap) {"
        )
    else:
        out(f"size_t caipo_protocol_encode_{lower}(uint8_t seq, uint8_t *out, size_t out_cap) {{")

    min_size = 2 + sum(_field_min_size(f) for f in message.fields)
    has_string = any(f.is_string for f in message.fields)

    if has_string:
        string_field = next(f for f in message.fields if f.is_string)
        out(f"    size_t needed = {min_size}u + msg->{string_field.name}_len;")
        out(f"    if (msg->{string_field.name}_len > {string_field.max_length}) {{")
        out("        return 0;")
        out("    }")
    else:
        out(f"    size_t needed = {min_size}u;")
    out("    if (out_cap < needed) {")
    out("        return 0;")
    out("    }")
    out("")
    out("    uint8_t *cursor = out;")
    out(f"    caipo__put_u8(&cursor, (uint8_t)CAIPO_MSG_{message.name});")
    out("    caipo__put_u8(&cursor, seq);")
    for field in message.fields:
        _render_field_put(out, field)
    out("    return (size_t)(cursor - out);")
    out("}")


def _render_field_put(out, field: Field) -> None:
    if field.is_string:
        out(f"    caipo__put_u8(&cursor, msg->{field.name}_len);")
        out(f"    memcpy(cursor, msg->{field.name}, msg->{field.name}_len);")
        out(f"    cursor += msg->{field.name}_len;")
    elif field.is_enum or field.type in ("u8", "i8", "bool"):
        out(f"    caipo__put_u8(&cursor, (uint8_t)msg->{field.name});")
    elif field.type in ("u16", "i16"):
        out(f"    caipo__put_u16le(&cursor, (uint16_t)msg->{field.name});")
    elif field.type in ("u32", "i32"):
        out(f"    caipo__put_u32le(&cursor, (uint32_t)msg->{field.name});")
    elif field.type == "f32":
        out(f"    caipo__put_f32le(&cursor, msg->{field.name});")
    else:
        raise AssertionError(f"unhandled field type {field.type!r}")


def _render_decode(out, message: Message, lower: str, struct_name: str | None) -> None:
    if struct_name is not None:
        out(
            f"bool caipo_protocol_decode_{lower}(const uint8_t *payload, size_t payload_len, "
            f"{struct_name} *out) {{"
        )
    else:
        out(f"bool caipo_protocol_decode_{lower}(const uint8_t *payload, size_t payload_len) {{")
        out("    (void)payload;")
        out("    return payload_len == 0;")
        out("}")
        return

    min_size = sum(_field_min_size(f) for f in message.fields)
    out(f"    if (payload_len < {min_size}u) {{")
    out("        return false;")
    out("    }")
    out("    const uint8_t *cursor = payload;")
    for field in message.fields:
        _render_field_get(out, message, field)
    out("    return true;")
    out("}")


def _render_field_get(out, message: Message, field: Field) -> None:
    if field.is_string:
        out(f"    uint8_t {field.name}_len = caipo__get_u8(&cursor);")
        out(f"    if ({field.name}_len > {field.max_length}) {{")
        out("        return false;")
        out("    }")
        out(
            f"    if ((size_t)(cursor - payload) + {field.name}_len > payload_len) {{"
        )
        out("        return false;")
        out("    }")
        out(f"    memcpy(out->{field.name}, cursor, {field.name}_len);")
        out(f"    out->{field.name}[{field.name}_len] = '\\0';")
        out(f"    out->{field.name}_len = {field.name}_len;")
        out(f"    cursor += {field.name}_len;")
    elif field.is_enum:
        out(f"    out->{field.name} = ({_enum_type_name(field.enum_name)})caipo__get_u8(&cursor);")
    elif field.type == "bool":
        out(f"    out->{field.name} = caipo__get_u8(&cursor) != 0;")
    elif field.type in ("u8", "i8"):
        out(f"    out->{field.name} = ({_C_PRIMITIVE[field.type]})caipo__get_u8(&cursor);")
    elif field.type in ("u16", "i16"):
        out(f"    out->{field.name} = ({_C_PRIMITIVE[field.type]})caipo__get_u16le(&cursor);")
    elif field.type in ("u32", "i32"):
        out(f"    out->{field.name} = ({_C_PRIMITIVE[field.type]})caipo__get_u32le(&cursor);")
    elif field.type == "f32":
        out(f"    out->{field.name} = caipo__get_f32le(&cursor);")
    else:
        raise AssertionError(f"unhandled field type {field.type!r}")
