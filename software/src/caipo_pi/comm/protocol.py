"""GENERATED FILE - DO NOT EDIT.

Produced by protocol/codegen/generate.py from protocol/caipo.protocol.yaml.
Edit the YAML and re-run codegen, never this file directly.
"""

from __future__ import annotations

import dataclasses
import enum
import struct


class ProtocolError(Exception):
    """Base class for protocol codec errors."""


class ProtocolEncodeError(ProtocolError):
    """Raised when a payload cannot be encoded (e.g. a field exceeds its bounds)."""


class ProtocolDecodeError(ProtocolError):
    """Raised when a byte buffer is not a valid encoded message."""


class MessageType(enum.IntEnum):
    HEARTBEAT = 1
    PI_READY = 2
    START_WORKFLOW = 3
    STOP_WORKFLOW = 4
    ACK = 5
    COMPLETE = 6
    ERROR = 7
    BATTERY_STATUS = 8
    BUTTON_EVENT = 9
    SHUTDOWN_REQUEST = 10
    SHUTDOWN_READY = 11


class WorkflowType(enum.IntEnum):
    QUICK_VIDEO = 1
    AUDIO_ONLY = 2
    SCHEDULED_WAKE = 3


class ButtonEvent(enum.IntEnum):
    TAP = 1
    DOUBLE_TAP = 2
    LONG_PRESS = 3


class ErrorCode(enum.IntEnum):
    UNKNOWN = 0
    BUSY = 1
    INVALID_PARAMS = 2
    HARDWARE_FAULT = 3
    TIMEOUT = 4
    STORAGE_FULL = 5


class ShutdownReason(enum.IntEnum):
    USER = 0
    LOW_BATTERY = 1
    SCHEDULED = 2


@dataclasses.dataclass(frozen=True)
class HeartbeatPayload:
    uptime_ms: int


@dataclasses.dataclass(frozen=True)
class PiReadyPayload:
    fw_major: int
    fw_minor: int
    fw_patch: int


@dataclasses.dataclass(frozen=True)
class StartWorkflowPayload:
    workflow: WorkflowType
    duration_s: int
    scheduled_delay_s: int


@dataclasses.dataclass(frozen=True)
class AckPayload:
    ack_seq: int


@dataclasses.dataclass(frozen=True)
class CompletePayload:
    ack_seq: int
    file_count: int
    bytes_captured: int


@dataclasses.dataclass(frozen=True)
class ErrorPayload:
    ack_seq: int
    code: ErrorCode
    message: str


@dataclasses.dataclass(frozen=True)
class BatteryStatusPayload:
    percent: int
    voltage_mv: int
    charging: bool


@dataclasses.dataclass(frozen=True)
class ButtonEventPayload:
    event: ButtonEvent


@dataclasses.dataclass(frozen=True)
class ShutdownRequestPayload:
    reason: ShutdownReason


def _encode_payload_heartbeat(payload: HeartbeatPayload) -> bytes:
    body = bytearray()
    body += struct.pack("<I", payload.uptime_ms)
    return bytes(body)


def _decode_payload_heartbeat(data: bytes) -> HeartbeatPayload:
    offset = 0
    try:
        (uptime_ms,) = struct.unpack_from("<I", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("HEARTBEAT.uptime_ms: buffer too short") from exc
    offset += 4
    return HeartbeatPayload(uptime_ms=uptime_ms)


def _encode_payload_pi_ready(payload: PiReadyPayload) -> bytes:
    body = bytearray()
    body += struct.pack("<B", payload.fw_major)
    body += struct.pack("<B", payload.fw_minor)
    body += struct.pack("<B", payload.fw_patch)
    return bytes(body)


def _decode_payload_pi_ready(data: bytes) -> PiReadyPayload:
    offset = 0
    try:
        (fw_major,) = struct.unpack_from("<B", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("PI_READY.fw_major: buffer too short") from exc
    offset += 1
    try:
        (fw_minor,) = struct.unpack_from("<B", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("PI_READY.fw_minor: buffer too short") from exc
    offset += 1
    try:
        (fw_patch,) = struct.unpack_from("<B", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("PI_READY.fw_patch: buffer too short") from exc
    offset += 1
    return PiReadyPayload(fw_major=fw_major, fw_minor=fw_minor, fw_patch=fw_patch)


def _encode_payload_start_workflow(payload: StartWorkflowPayload) -> bytes:
    body = bytearray()
    body += struct.pack("<B", int(payload.workflow))
    body += struct.pack("<H", payload.duration_s)
    body += struct.pack("<H", payload.scheduled_delay_s)
    return bytes(body)


def _decode_payload_start_workflow(data: bytes) -> StartWorkflowPayload:
    offset = 0
    try:
        (_raw,) = struct.unpack_from("<B", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("START_WORKFLOW.workflow: buffer too short") from exc
    offset += 1
    try:
        workflow = WorkflowType(_raw)
    except ValueError as exc:
        raise ProtocolDecodeError(f"START_WORKFLOW.workflow: unknown WorkflowType value {_raw}") from exc
    try:
        (duration_s,) = struct.unpack_from("<H", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("START_WORKFLOW.duration_s: buffer too short") from exc
    offset += 2
    try:
        (scheduled_delay_s,) = struct.unpack_from("<H", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("START_WORKFLOW.scheduled_delay_s: buffer too short") from exc
    offset += 2
    return StartWorkflowPayload(workflow=workflow, duration_s=duration_s, scheduled_delay_s=scheduled_delay_s)


def _encode_payload_stop_workflow(payload: None) -> bytes:
    """STOP_WORKFLOW carries no payload."""
    return b""


def _decode_payload_stop_workflow(data: bytes) -> None:
    """STOP_WORKFLOW carries no payload."""
    return None


def _encode_payload_ack(payload: AckPayload) -> bytes:
    body = bytearray()
    body += struct.pack("<B", payload.ack_seq)
    return bytes(body)


def _decode_payload_ack(data: bytes) -> AckPayload:
    offset = 0
    try:
        (ack_seq,) = struct.unpack_from("<B", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("ACK.ack_seq: buffer too short") from exc
    offset += 1
    return AckPayload(ack_seq=ack_seq)


def _encode_payload_complete(payload: CompletePayload) -> bytes:
    body = bytearray()
    body += struct.pack("<B", payload.ack_seq)
    body += struct.pack("<B", payload.file_count)
    body += struct.pack("<I", payload.bytes_captured)
    return bytes(body)


def _decode_payload_complete(data: bytes) -> CompletePayload:
    offset = 0
    try:
        (ack_seq,) = struct.unpack_from("<B", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("COMPLETE.ack_seq: buffer too short") from exc
    offset += 1
    try:
        (file_count,) = struct.unpack_from("<B", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("COMPLETE.file_count: buffer too short") from exc
    offset += 1
    try:
        (bytes_captured,) = struct.unpack_from("<I", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("COMPLETE.bytes_captured: buffer too short") from exc
    offset += 4
    return CompletePayload(ack_seq=ack_seq, file_count=file_count, bytes_captured=bytes_captured)


def _encode_payload_error(payload: ErrorPayload) -> bytes:
    body = bytearray()
    body += struct.pack("<B", payload.ack_seq)
    body += struct.pack("<B", int(payload.code))
    message_bytes = payload.message.encode("utf-8")
    if len(message_bytes) > 64:
        raise ProtocolEncodeError(f"ERROR.message exceeds max_length 64 ({len(message_bytes)} bytes)")
    body += struct.pack("<B", len(message_bytes))
    body += message_bytes
    return bytes(body)


def _decode_payload_error(data: bytes) -> ErrorPayload:
    offset = 0
    try:
        (ack_seq,) = struct.unpack_from("<B", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("ERROR.ack_seq: buffer too short") from exc
    offset += 1
    try:
        (_raw,) = struct.unpack_from("<B", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("ERROR.code: buffer too short") from exc
    offset += 1
    try:
        code = ErrorCode(_raw)
    except ValueError as exc:
        raise ProtocolDecodeError(f"ERROR.code: unknown ErrorCode value {_raw}") from exc
    try:
        (_len,) = struct.unpack_from("<B", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("ERROR.message: buffer too short for length prefix") from exc
    offset += 1
    _raw = data[offset:offset + _len]
    if len(_raw) != _len:
        raise ProtocolDecodeError("ERROR.message: buffer truncated, declared length exceeds available data")
    offset += _len
    try:
        message = _raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ProtocolDecodeError("ERROR.message: invalid utf-8") from exc
    return ErrorPayload(ack_seq=ack_seq, code=code, message=message)


def _encode_payload_battery_status(payload: BatteryStatusPayload) -> bytes:
    body = bytearray()
    body += struct.pack("<B", payload.percent)
    body += struct.pack("<H", payload.voltage_mv)
    body += struct.pack("<B", 1 if payload.charging else 0)
    return bytes(body)


def _decode_payload_battery_status(data: bytes) -> BatteryStatusPayload:
    offset = 0
    try:
        (percent,) = struct.unpack_from("<B", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("BATTERY_STATUS.percent: buffer too short") from exc
    offset += 1
    try:
        (voltage_mv,) = struct.unpack_from("<H", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("BATTERY_STATUS.voltage_mv: buffer too short") from exc
    offset += 2
    try:
        (_raw,) = struct.unpack_from("<B", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("BATTERY_STATUS.charging: buffer too short") from exc
    charging = _raw != 0
    offset += 1
    return BatteryStatusPayload(percent=percent, voltage_mv=voltage_mv, charging=charging)


def _encode_payload_button_event(payload: ButtonEventPayload) -> bytes:
    body = bytearray()
    body += struct.pack("<B", int(payload.event))
    return bytes(body)


def _decode_payload_button_event(data: bytes) -> ButtonEventPayload:
    offset = 0
    try:
        (_raw,) = struct.unpack_from("<B", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("BUTTON_EVENT.event: buffer too short") from exc
    offset += 1
    try:
        event = ButtonEvent(_raw)
    except ValueError as exc:
        raise ProtocolDecodeError(f"BUTTON_EVENT.event: unknown ButtonEvent value {_raw}") from exc
    return ButtonEventPayload(event=event)


def _encode_payload_shutdown_request(payload: ShutdownRequestPayload) -> bytes:
    body = bytearray()
    body += struct.pack("<B", int(payload.reason))
    return bytes(body)


def _decode_payload_shutdown_request(data: bytes) -> ShutdownRequestPayload:
    offset = 0
    try:
        (_raw,) = struct.unpack_from("<B", data, offset)
    except struct.error as exc:
        raise ProtocolDecodeError("SHUTDOWN_REQUEST.reason: buffer too short") from exc
    offset += 1
    try:
        reason = ShutdownReason(_raw)
    except ValueError as exc:
        raise ProtocolDecodeError(f"SHUTDOWN_REQUEST.reason: unknown ShutdownReason value {_raw}") from exc
    return ShutdownRequestPayload(reason=reason)


def _encode_payload_shutdown_ready(payload: None) -> bytes:
    """SHUTDOWN_READY carries no payload."""
    return b""


def _decode_payload_shutdown_ready(data: bytes) -> None:
    """SHUTDOWN_READY carries no payload."""
    return None


def encode_heartbeat(seq: int, payload: HeartbeatPayload) -> bytes:
    body = _encode_payload_heartbeat(payload)
    return bytes([int(MessageType.HEARTBEAT), seq & 0xFF]) + body


def decode_heartbeat(data: bytes) -> tuple[int, HeartbeatPayload]:
    if len(data) < 2:
        raise ProtocolDecodeError("buffer too short for envelope")
    if data[0] != int(MessageType.HEARTBEAT):
        raise ProtocolDecodeError(f"expected msg_type HEARTBEAT (1), got {data[0]}")
    seq = data[1]
    payload = _decode_payload_heartbeat(data[2:])
    return seq, payload


def encode_pi_ready(seq: int, payload: PiReadyPayload) -> bytes:
    body = _encode_payload_pi_ready(payload)
    return bytes([int(MessageType.PI_READY), seq & 0xFF]) + body


def decode_pi_ready(data: bytes) -> tuple[int, PiReadyPayload]:
    if len(data) < 2:
        raise ProtocolDecodeError("buffer too short for envelope")
    if data[0] != int(MessageType.PI_READY):
        raise ProtocolDecodeError(f"expected msg_type PI_READY (2), got {data[0]}")
    seq = data[1]
    payload = _decode_payload_pi_ready(data[2:])
    return seq, payload


def encode_start_workflow(seq: int, payload: StartWorkflowPayload) -> bytes:
    body = _encode_payload_start_workflow(payload)
    return bytes([int(MessageType.START_WORKFLOW), seq & 0xFF]) + body


def decode_start_workflow(data: bytes) -> tuple[int, StartWorkflowPayload]:
    if len(data) < 2:
        raise ProtocolDecodeError("buffer too short for envelope")
    if data[0] != int(MessageType.START_WORKFLOW):
        raise ProtocolDecodeError(f"expected msg_type START_WORKFLOW (3), got {data[0]}")
    seq = data[1]
    payload = _decode_payload_start_workflow(data[2:])
    return seq, payload


def encode_stop_workflow(seq: int) -> bytes:
    body = _encode_payload_stop_workflow(None)
    return bytes([int(MessageType.STOP_WORKFLOW), seq & 0xFF]) + body


def decode_stop_workflow(data: bytes) -> tuple[int, None]:
    if len(data) < 2:
        raise ProtocolDecodeError("buffer too short for envelope")
    if data[0] != int(MessageType.STOP_WORKFLOW):
        raise ProtocolDecodeError(f"expected msg_type STOP_WORKFLOW (4), got {data[0]}")
    seq = data[1]
    payload = _decode_payload_stop_workflow(data[2:])
    return seq, payload


def encode_ack(seq: int, payload: AckPayload) -> bytes:
    body = _encode_payload_ack(payload)
    return bytes([int(MessageType.ACK), seq & 0xFF]) + body


def decode_ack(data: bytes) -> tuple[int, AckPayload]:
    if len(data) < 2:
        raise ProtocolDecodeError("buffer too short for envelope")
    if data[0] != int(MessageType.ACK):
        raise ProtocolDecodeError(f"expected msg_type ACK (5), got {data[0]}")
    seq = data[1]
    payload = _decode_payload_ack(data[2:])
    return seq, payload


def encode_complete(seq: int, payload: CompletePayload) -> bytes:
    body = _encode_payload_complete(payload)
    return bytes([int(MessageType.COMPLETE), seq & 0xFF]) + body


def decode_complete(data: bytes) -> tuple[int, CompletePayload]:
    if len(data) < 2:
        raise ProtocolDecodeError("buffer too short for envelope")
    if data[0] != int(MessageType.COMPLETE):
        raise ProtocolDecodeError(f"expected msg_type COMPLETE (6), got {data[0]}")
    seq = data[1]
    payload = _decode_payload_complete(data[2:])
    return seq, payload


def encode_error(seq: int, payload: ErrorPayload) -> bytes:
    body = _encode_payload_error(payload)
    return bytes([int(MessageType.ERROR), seq & 0xFF]) + body


def decode_error(data: bytes) -> tuple[int, ErrorPayload]:
    if len(data) < 2:
        raise ProtocolDecodeError("buffer too short for envelope")
    if data[0] != int(MessageType.ERROR):
        raise ProtocolDecodeError(f"expected msg_type ERROR (7), got {data[0]}")
    seq = data[1]
    payload = _decode_payload_error(data[2:])
    return seq, payload


def encode_battery_status(seq: int, payload: BatteryStatusPayload) -> bytes:
    body = _encode_payload_battery_status(payload)
    return bytes([int(MessageType.BATTERY_STATUS), seq & 0xFF]) + body


def decode_battery_status(data: bytes) -> tuple[int, BatteryStatusPayload]:
    if len(data) < 2:
        raise ProtocolDecodeError("buffer too short for envelope")
    if data[0] != int(MessageType.BATTERY_STATUS):
        raise ProtocolDecodeError(f"expected msg_type BATTERY_STATUS (8), got {data[0]}")
    seq = data[1]
    payload = _decode_payload_battery_status(data[2:])
    return seq, payload


def encode_button_event(seq: int, payload: ButtonEventPayload) -> bytes:
    body = _encode_payload_button_event(payload)
    return bytes([int(MessageType.BUTTON_EVENT), seq & 0xFF]) + body


def decode_button_event(data: bytes) -> tuple[int, ButtonEventPayload]:
    if len(data) < 2:
        raise ProtocolDecodeError("buffer too short for envelope")
    if data[0] != int(MessageType.BUTTON_EVENT):
        raise ProtocolDecodeError(f"expected msg_type BUTTON_EVENT (9), got {data[0]}")
    seq = data[1]
    payload = _decode_payload_button_event(data[2:])
    return seq, payload


def encode_shutdown_request(seq: int, payload: ShutdownRequestPayload) -> bytes:
    body = _encode_payload_shutdown_request(payload)
    return bytes([int(MessageType.SHUTDOWN_REQUEST), seq & 0xFF]) + body


def decode_shutdown_request(data: bytes) -> tuple[int, ShutdownRequestPayload]:
    if len(data) < 2:
        raise ProtocolDecodeError("buffer too short for envelope")
    if data[0] != int(MessageType.SHUTDOWN_REQUEST):
        raise ProtocolDecodeError(f"expected msg_type SHUTDOWN_REQUEST (10), got {data[0]}")
    seq = data[1]
    payload = _decode_payload_shutdown_request(data[2:])
    return seq, payload


def encode_shutdown_ready(seq: int) -> bytes:
    body = _encode_payload_shutdown_ready(None)
    return bytes([int(MessageType.SHUTDOWN_READY), seq & 0xFF]) + body


def decode_shutdown_ready(data: bytes) -> tuple[int, None]:
    if len(data) < 2:
        raise ProtocolDecodeError("buffer too short for envelope")
    if data[0] != int(MessageType.SHUTDOWN_READY):
        raise ProtocolDecodeError(f"expected msg_type SHUTDOWN_READY (11), got {data[0]}")
    seq = data[1]
    payload = _decode_payload_shutdown_ready(data[2:])
    return seq, payload


_PAYLOAD_ENCODERS = {
    MessageType.HEARTBEAT: _encode_payload_heartbeat,
    MessageType.PI_READY: _encode_payload_pi_ready,
    MessageType.START_WORKFLOW: _encode_payload_start_workflow,
    MessageType.STOP_WORKFLOW: _encode_payload_stop_workflow,
    MessageType.ACK: _encode_payload_ack,
    MessageType.COMPLETE: _encode_payload_complete,
    MessageType.ERROR: _encode_payload_error,
    MessageType.BATTERY_STATUS: _encode_payload_battery_status,
    MessageType.BUTTON_EVENT: _encode_payload_button_event,
    MessageType.SHUTDOWN_REQUEST: _encode_payload_shutdown_request,
    MessageType.SHUTDOWN_READY: _encode_payload_shutdown_ready,
}

_PAYLOAD_DECODERS = {
    MessageType.HEARTBEAT: _decode_payload_heartbeat,
    MessageType.PI_READY: _decode_payload_pi_ready,
    MessageType.START_WORKFLOW: _decode_payload_start_workflow,
    MessageType.STOP_WORKFLOW: _decode_payload_stop_workflow,
    MessageType.ACK: _decode_payload_ack,
    MessageType.COMPLETE: _decode_payload_complete,
    MessageType.ERROR: _decode_payload_error,
    MessageType.BATTERY_STATUS: _decode_payload_battery_status,
    MessageType.BUTTON_EVENT: _decode_payload_button_event,
    MessageType.SHUTDOWN_REQUEST: _decode_payload_shutdown_request,
    MessageType.SHUTDOWN_READY: _decode_payload_shutdown_ready,
}


@dataclasses.dataclass(frozen=True)
class DecodedMessage:
    msg_type: MessageType
    seq: int
    payload: object


def encode(msg_type: MessageType, seq: int, payload: object = None) -> bytes:
    try:
        encoder = _PAYLOAD_ENCODERS[msg_type]
    except KeyError as exc:
        raise ProtocolEncodeError(f"unknown message type {msg_type!r}") from exc
    body = encoder(payload)
    return bytes([int(msg_type), seq & 0xFF]) + body


def decode(data: bytes) -> DecodedMessage:
    if len(data) < 2:
        raise ProtocolDecodeError("buffer too short for envelope")
    raw_type = data[0]
    try:
        msg_type = MessageType(raw_type)
    except ValueError as exc:
        raise ProtocolDecodeError(f"unknown message type {raw_type}") from exc
    seq = data[1]
    decoder = _PAYLOAD_DECODERS[msg_type]
    payload = decoder(data[2:])
    return DecodedMessage(msg_type=msg_type, seq=seq, payload=payload)
