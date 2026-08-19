"""Renders software/src/caipo_pi/comm/protocol.py from a validated Protocol."""

from __future__ import annotations

from schema import Field, Message, Protocol, to_lower_snake_case, to_pascal_case

_STRUCT_FMT = {
    "u8": "B",
    "i8": "b",
    "u16": "H",
    "i16": "h",
    "u32": "I",
    "i32": "i",
    "f32": "f",
    "bool": "B",
}

_STRUCT_SIZE = {
    "u8": 1,
    "i8": 1,
    "u16": 2,
    "i16": 2,
    "u32": 4,
    "i32": 4,
    "f32": 4,
    "bool": 1,
}


def _python_type(field: Field) -> str:
    if field.is_enum:
        return field.enum_name
    if field.is_string:
        return "str"
    if field.type == "bool":
        return "bool"
    if field.type == "f32":
        return "float"
    return "int"


def _payload_class_name(message: Message) -> str:
    return f"{to_pascal_case(message.name)}Payload"


def render(protocol: Protocol, source_path: str) -> str:
    lines: list[str] = []
    out = lines.append

    out('"""GENERATED FILE - DO NOT EDIT.')
    out("")
    out(f"Produced by protocol/codegen/generate.py from {source_path}.")
    out("Edit the YAML and re-run codegen, never this file directly.")
    out('"""')
    out("")
    out("from __future__ import annotations")
    out("")
    out("import dataclasses")
    out("import enum")
    out("import struct")
    out("")
    out("")
    out("class ProtocolError(Exception):")
    out('    """Base class for protocol codec errors."""')
    out("")
    out("")
    out("class ProtocolEncodeError(ProtocolError):")
    out('    """Raised when a payload cannot be encoded (e.g. a field exceeds its bounds)."""')
    out("")
    out("")
    out("class ProtocolDecodeError(ProtocolError):")
    out('    """Raised when a byte buffer is not a valid encoded message."""')
    out("")
    out("")

    _render_message_type_enum(out, protocol)
    for enum_def in protocol.enums.values():
        out(f"class {enum_def.name}(enum.IntEnum):")
        for member_name, value in enum_def.values.items():
            out(f"    {member_name} = {value}")
        out("")
        out("")

    for message in protocol.messages:
        if message.has_payload:
            _render_payload_dataclass(out, message)

    for message in protocol.messages:
        _render_payload_codec(out, message)

    for message in protocol.messages:
        _render_message_codec(out, message)

    _render_dispatch_tables(out, protocol)
    _render_generic_codec(out)

    return "\n".join(lines) + "\n"


def _render_message_type_enum(out, protocol: Protocol) -> None:
    out("class MessageType(enum.IntEnum):")
    for message in protocol.messages:
        out(f"    {message.name} = {message.id}")
    out("")
    out("")


def _render_payload_dataclass(out, message: Message) -> None:
    class_name = _payload_class_name(message)
    out("@dataclasses.dataclass(frozen=True)")
    out(f"class {class_name}:")
    for field in message.fields:
        out(f"    {field.name}: {_python_type(field)}")
    out("")
    out("")


def _render_payload_codec(out, message: Message) -> None:
    lower = to_lower_snake_case(message.name)
    class_name = _payload_class_name(message) if message.has_payload else "None"

    out(f"def _encode_payload_{lower}(payload: {class_name}) -> bytes:")
    if not message.has_payload:
        out('    """{} carries no payload."""'.format(message.name))
        out("    return b\"\"")
    else:
        out("    body = bytearray()")
        for field in message.fields:
            _render_field_encode(out, message, field)
        out("    return bytes(body)")
    out("")
    out("")

    out(f"def _decode_payload_{lower}(data: bytes) -> {class_name}:")
    if not message.has_payload:
        out(f'    """{message.name} carries no payload."""')
        out("    return None")
    else:
        out("    offset = 0")
        for field in message.fields:
            _render_field_decode(out, message, field)
        args = ", ".join(f"{f.name}={f.name}" for f in message.fields)
        out(f"    return {class_name}({args})")
    out("")
    out("")


def _render_field_encode(out, message: Message, field: Field) -> None:
    if field.is_string:
        out(f"    {field.name}_bytes = payload.{field.name}.encode(\"utf-8\")")
        out(f"    if len({field.name}_bytes) > {field.max_length}:")
        out(
            f"        raise ProtocolEncodeError("
            f"f\"{message.name}.{field.name} exceeds max_length {field.max_length} "
            f"({{len({field.name}_bytes)}} bytes)\")"
        )
        out(f"    body += struct.pack(\"<B\", len({field.name}_bytes))")
        out(f"    body += {field.name}_bytes")
    elif field.is_enum:
        out(f"    body += struct.pack(\"<B\", int(payload.{field.name}))")
    elif field.type == "bool":
        out(f"    body += struct.pack(\"<B\", 1 if payload.{field.name} else 0)")
    else:
        fmt = _STRUCT_FMT[field.type]
        out(f"    body += struct.pack(\"<{fmt}\", payload.{field.name})")


def _render_field_decode(out, message: Message, field: Field) -> None:
    ctx = f"{message.name}.{field.name}"
    if field.is_string:
        out("    try:")
        out("        (_len,) = struct.unpack_from(\"<B\", data, offset)")
        out("    except struct.error as exc:")
        out(f"        raise ProtocolDecodeError(\"{ctx}: buffer too short for length prefix\") from exc")
        out("    offset += 1")
        out("    _raw = data[offset:offset + _len]")
        out("    if len(_raw) != _len:")
        out(f"        raise ProtocolDecodeError(\"{ctx}: buffer truncated, declared length exceeds available data\")")
        out("    offset += _len")
        out("    try:")
        out(f"        {field.name} = _raw.decode(\"utf-8\")")
        out("    except UnicodeDecodeError as exc:")
        out(f"        raise ProtocolDecodeError(\"{ctx}: invalid utf-8\") from exc")
    elif field.is_enum:
        out("    try:")
        out("        (_raw,) = struct.unpack_from(\"<B\", data, offset)")
        out("    except struct.error as exc:")
        out(f"        raise ProtocolDecodeError(\"{ctx}: buffer too short\") from exc")
        out("    offset += 1")
        out("    try:")
        out(f"        {field.name} = {field.enum_name}(_raw)")
        out("    except ValueError as exc:")
        out(f"        raise ProtocolDecodeError(f\"{ctx}: unknown {field.enum_name} value {{_raw}}\") from exc")
    elif field.type == "bool":
        out("    try:")
        out("        (_raw,) = struct.unpack_from(\"<B\", data, offset)")
        out("    except struct.error as exc:")
        out(f"        raise ProtocolDecodeError(\"{ctx}: buffer too short\") from exc")
        out(f"    {field.name} = _raw != 0")
        out("    offset += 1")
    else:
        fmt = _STRUCT_FMT[field.type]
        size = _STRUCT_SIZE[field.type]
        out("    try:")
        out(f"        ({field.name},) = struct.unpack_from(\"<{fmt}\", data, offset)")
        out("    except struct.error as exc:")
        out(f"        raise ProtocolDecodeError(\"{ctx}: buffer too short\") from exc")
        out(f"    offset += {size}")


def _render_message_codec(out, message: Message) -> None:
    lower = to_lower_snake_case(message.name)

    if message.has_payload:
        class_name = _payload_class_name(message)
        out(f"def encode_{lower}(seq: int, payload: {class_name}) -> bytes:")
        out(f"    body = _encode_payload_{lower}(payload)")
        out(f"    return bytes([int(MessageType.{message.name}), seq & 0xFF]) + body")
        out("")
        out("")
        out(f"def decode_{lower}(data: bytes) -> tuple[int, {class_name}]:")
    else:
        out(f"def encode_{lower}(seq: int) -> bytes:")
        out(f"    body = _encode_payload_{lower}(None)")
        out(f"    return bytes([int(MessageType.{message.name}), seq & 0xFF]) + body")
        out("")
        out("")
        out(f"def decode_{lower}(data: bytes) -> tuple[int, None]:")

    out("    if len(data) < 2:")
    out("        raise ProtocolDecodeError(\"buffer too short for envelope\")")
    out(f"    if data[0] != int(MessageType.{message.name}):")
    out(
        f"        raise ProtocolDecodeError("
        f"f\"expected msg_type {message.name} ({message.id}), got {{data[0]}}\")"
    )
    out("    seq = data[1]")
    out(f"    payload = _decode_payload_{lower}(data[2:])")
    out("    return seq, payload")
    out("")
    out("")


def _render_dispatch_tables(out, protocol: Protocol) -> None:
    out("_PAYLOAD_ENCODERS = {")
    for message in protocol.messages:
        lower = to_lower_snake_case(message.name)
        out(f"    MessageType.{message.name}: _encode_payload_{lower},")
    out("}")
    out("")
    out("_PAYLOAD_DECODERS = {")
    for message in protocol.messages:
        lower = to_lower_snake_case(message.name)
        out(f"    MessageType.{message.name}: _decode_payload_{lower},")
    out("}")
    out("")
    out("")


def _render_generic_codec(out) -> None:
    out("@dataclasses.dataclass(frozen=True)")
    out("class DecodedMessage:")
    out("    msg_type: MessageType")
    out("    seq: int")
    out("    payload: object")
    out("")
    out("")
    out("def encode(msg_type: MessageType, seq: int, payload: object = None) -> bytes:")
    out("    try:")
    out("        encoder = _PAYLOAD_ENCODERS[msg_type]")
    out("    except KeyError as exc:")
    out("        raise ProtocolEncodeError(f\"unknown message type {msg_type!r}\") from exc")
    out("    body = encoder(payload)")
    out("    return bytes([int(msg_type), seq & 0xFF]) + body")
    out("")
    out("")
    out("def decode(data: bytes) -> DecodedMessage:")
    out("    if len(data) < 2:")
    out("        raise ProtocolDecodeError(\"buffer too short for envelope\")")
    out("    raw_type = data[0]")
    out("    try:")
    out("        msg_type = MessageType(raw_type)")
    out("    except ValueError as exc:")
    out("        raise ProtocolDecodeError(f\"unknown message type {raw_type}\") from exc")
    out("    seq = data[1]")
    out("    decoder = _PAYLOAD_DECODERS[msg_type]")
    out("    payload = decoder(data[2:])")
    out("    return DecodedMessage(msg_type=msg_type, seq=seq, payload=payload)")
