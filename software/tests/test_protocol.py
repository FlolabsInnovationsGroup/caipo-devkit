"""Round-trips comm/protocol.py against the golden vectors in /protocol/vectors/.

These are the same vectors firmware/test/host/test_protocol_host.c runs against,
so a passing suite on both sides means the two generated codecs agree byte-for-byte,
not just that each is internally consistent.
"""

from __future__ import annotations

import dataclasses
import json
from pathlib import Path
from typing import Callable

import pytest

from caipo_pi.comm import protocol

REPO_ROOT = Path(__file__).resolve().parents[2]
VECTORS_DIR = REPO_ROOT / "protocol" / "vectors"


@dataclasses.dataclass(frozen=True)
class MessageSpec:
    msg_type: protocol.MessageType
    encode: Callable[..., bytes]
    decode: Callable[[bytes], tuple]
    payload_cls: type | None
    enum_fields: dict[str, type]


MESSAGE_SPECS: dict[str, MessageSpec] = {
    "heartbeat": MessageSpec(
        protocol.MessageType.HEARTBEAT, protocol.encode_heartbeat, protocol.decode_heartbeat,
        protocol.HeartbeatPayload, {},
    ),
    "pi_ready": MessageSpec(
        protocol.MessageType.PI_READY, protocol.encode_pi_ready, protocol.decode_pi_ready,
        protocol.PiReadyPayload, {},
    ),
    "start_workflow": MessageSpec(
        protocol.MessageType.START_WORKFLOW, protocol.encode_start_workflow, protocol.decode_start_workflow,
        protocol.StartWorkflowPayload, {"workflow": protocol.WorkflowType},
    ),
    "stop_workflow": MessageSpec(
        protocol.MessageType.STOP_WORKFLOW, protocol.encode_stop_workflow, protocol.decode_stop_workflow,
        None, {},
    ),
    "ack": MessageSpec(
        protocol.MessageType.ACK, protocol.encode_ack, protocol.decode_ack,
        protocol.AckPayload, {},
    ),
    "complete": MessageSpec(
        protocol.MessageType.COMPLETE, protocol.encode_complete, protocol.decode_complete,
        protocol.CompletePayload, {},
    ),
    "error": MessageSpec(
        protocol.MessageType.ERROR, protocol.encode_error, protocol.decode_error,
        protocol.ErrorPayload, {"code": protocol.ErrorCode},
    ),
    "battery_status": MessageSpec(
        protocol.MessageType.BATTERY_STATUS, protocol.encode_battery_status, protocol.decode_battery_status,
        protocol.BatteryStatusPayload, {},
    ),
    "button_event": MessageSpec(
        protocol.MessageType.BUTTON_EVENT, protocol.encode_button_event, protocol.decode_button_event,
        protocol.ButtonEventPayload, {"event": protocol.ButtonEvent},
    ),
    "shutdown_request": MessageSpec(
        protocol.MessageType.SHUTDOWN_REQUEST, protocol.encode_shutdown_request, protocol.decode_shutdown_request,
        protocol.ShutdownRequestPayload, {"reason": protocol.ShutdownReason},
    ),
    "shutdown_ready": MessageSpec(
        protocol.MessageType.SHUTDOWN_READY, protocol.encode_shutdown_ready, protocol.decode_shutdown_ready,
        None, {},
    ),
}


def _build_payload(spec: MessageSpec, fields: dict) -> object:
    if spec.payload_cls is None:
        return None
    kwargs = dict(fields)
    for field_name, enum_cls in spec.enum_fields.items():
        kwargs[field_name] = enum_cls[kwargs[field_name]]
    return spec.payload_cls(**kwargs)


def _load_cases() -> list:
    cases = []
    for name, spec in MESSAGE_SPECS.items():
        vectors = json.loads((VECTORS_DIR / f"{name}.json").read_text())
        for vector in vectors:
            cases.append(pytest.param(name, spec, vector, id=f"{name}/{vector['description']}"))
    return cases


CASES = _load_cases()


def test_all_vector_files_are_covered():
    vector_names = {p.stem for p in VECTORS_DIR.glob("*.json")}
    assert vector_names == set(MESSAGE_SPECS)


@pytest.mark.parametrize("name,spec,vector", CASES)
def test_encode_matches_golden_hex(name, spec, vector):
    payload = _build_payload(spec, vector["fields"])
    expected = bytes.fromhex(vector["hex"])
    encoded = spec.encode(vector["seq"]) if spec.payload_cls is None else spec.encode(vector["seq"], payload)
    assert encoded == expected


@pytest.mark.parametrize("name,spec,vector", CASES)
def test_decode_matches_golden_fields(name, spec, vector):
    data = bytes.fromhex(vector["hex"])
    seq, payload = spec.decode(data)
    assert seq == vector["seq"]
    assert payload == _build_payload(spec, vector["fields"])


@pytest.mark.parametrize("name,spec,vector", CASES)
def test_generic_dispatch_matches_specific_codec(name, spec, vector):
    payload = _build_payload(spec, vector["fields"])
    expected = bytes.fromhex(vector["hex"])

    encoded = protocol.encode(spec.msg_type, vector["seq"], payload)
    assert encoded == expected

    decoded = protocol.decode(expected)
    assert decoded.msg_type == spec.msg_type
    assert decoded.seq == vector["seq"]
    assert decoded.payload == payload


@pytest.mark.parametrize("name,spec,vector", CASES)
def test_decode_rejects_truncated_buffer(name, spec, vector):
    data = bytes.fromhex(vector["hex"])
    if len(data) <= 2:
        pytest.skip("no payload to truncate")
    with pytest.raises(protocol.ProtocolDecodeError):
        spec.decode(data[:-1])
