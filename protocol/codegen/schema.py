"""Parses and validates protocol/caipo.protocol.yaml into structured objects.

Shared by render_python.py and render_c.py so both codecs are generated from
the same validated in-memory representation, never from the raw YAML dict.
"""

from __future__ import annotations

import dataclasses

PRIMITIVE_SIZES: dict[str, int] = {
    "u8": 1,
    "i8": 1,
    "u16": 2,
    "i16": 2,
    "u32": 4,
    "i32": 4,
    "f32": 4,
    "bool": 1,
}

VALID_DIRECTIONS = {"esp_to_pi", "pi_to_esp", "either"}

DEFAULT_STRING_MAX_LENGTH = 64


class SchemaError(Exception):
    """Raised when caipo.protocol.yaml fails validation."""


@dataclasses.dataclass(frozen=True)
class EnumDef:
    name: str
    values: dict[str, int]  # member name -> value, all in 0..255


@dataclasses.dataclass(frozen=True)
class Field:
    name: str
    type: str  # raw type token: "u8", "enum:WorkflowType", "string", ...
    max_length: int | None = None  # only set when type == "string"

    @property
    def is_enum(self) -> bool:
        return self.type.startswith("enum:")

    @property
    def enum_name(self) -> str:
        assert self.is_enum
        return self.type.split(":", 1)[1]

    @property
    def is_string(self) -> bool:
        return self.type == "string"

    @property
    def is_primitive(self) -> bool:
        return self.type in PRIMITIVE_SIZES


@dataclasses.dataclass(frozen=True)
class Message:
    name: str
    id: int
    direction: str
    fields: list[Field]

    @property
    def has_payload(self) -> bool:
        return len(self.fields) > 0


@dataclasses.dataclass(frozen=True)
class Protocol:
    version: int
    enums: dict[str, EnumDef]
    messages: list[Message]


def load_protocol(raw: dict) -> Protocol:
    if "version" not in raw:
        raise SchemaError("missing top-level 'version'")

    enums = _parse_enums(raw.get("enums", {}))
    messages = _parse_messages(raw.get("messages", []), enums)

    return Protocol(version=raw["version"], enums=enums, messages=messages)


def _parse_enums(enums_raw: dict) -> dict[str, EnumDef]:
    enums: dict[str, EnumDef] = {}
    for enum_name, members in enums_raw.items():
        if not members:
            raise SchemaError(f"enum '{enum_name}' has no members")
        seen_values: set[int] = set()
        for member_name, value in members.items():
            if not isinstance(value, int) or not (0 <= value <= 255):
                raise SchemaError(
                    f"enum '{enum_name}' member '{member_name}' = {value!r} must be an integer in 0..255"
                )
            if value in seen_values:
                raise SchemaError(f"enum '{enum_name}' has duplicate value {value}")
            seen_values.add(value)
        enums[enum_name] = EnumDef(name=enum_name, values=dict(members))
    return enums


def _parse_messages(messages_raw: list, enums: dict[str, EnumDef]) -> list[Message]:
    if not messages_raw:
        raise SchemaError("no messages defined")

    messages: list[Message] = []
    seen_names: set[str] = set()
    seen_ids: set[int] = set()

    for msg_raw in messages_raw:
        name = msg_raw["name"]
        msg_id = msg_raw["id"]
        direction = msg_raw.get("direction", "either")

        if name in seen_names:
            raise SchemaError(f"duplicate message name '{name}'")
        seen_names.add(name)

        if not isinstance(msg_id, int) or not (0 <= msg_id <= 255):
            raise SchemaError(f"message '{name}' id {msg_id!r} must be an integer in 0..255")
        if msg_id in seen_ids:
            raise SchemaError(f"duplicate message id {msg_id} (message '{name}')")
        seen_ids.add(msg_id)

        if direction not in VALID_DIRECTIONS:
            raise SchemaError(
                f"message '{name}' has unknown direction '{direction}' "
                f"(expected one of {sorted(VALID_DIRECTIONS)})"
            )

        fields = _parse_fields(name, msg_raw.get("fields", []), enums)
        messages.append(Message(name=name, id=msg_id, direction=direction, fields=fields))

    return messages


def _parse_fields(msg_name: str, fields_raw: list, enums: dict[str, EnumDef]) -> list[Field]:
    fields: list[Field] = []
    seen_field_names: set[str] = set()

    for field_raw in fields_raw:
        field_name = field_raw["name"]
        field_type = field_raw["type"]

        if field_name in seen_field_names:
            raise SchemaError(f"message '{msg_name}' has duplicate field '{field_name}'")
        seen_field_names.add(field_name)

        max_length = None
        if field_type.startswith("enum:"):
            enum_name = field_type.split(":", 1)[1]
            if enum_name not in enums:
                raise SchemaError(
                    f"message '{msg_name}' field '{field_name}' references unknown enum '{enum_name}'"
                )
        elif field_type == "string":
            max_length = field_raw.get("max_length", DEFAULT_STRING_MAX_LENGTH)
            if not isinstance(max_length, int) or not (1 <= max_length <= 255):
                raise SchemaError(
                    f"message '{msg_name}' field '{field_name}' max_length {max_length!r} "
                    "must be an integer in 1..255"
                )
        elif field_type not in PRIMITIVE_SIZES:
            raise SchemaError(
                f"message '{msg_name}' field '{field_name}' has unknown type '{field_type}'"
            )

        fields.append(Field(name=field_name, type=field_type, max_length=max_length))

    return fields


def to_pascal_case(screaming_snake: str) -> str:
    """START_WORKFLOW -> StartWorkflow"""
    return "".join(part.capitalize() for part in screaming_snake.split("_"))


def to_lower_snake_case(screaming_snake: str) -> str:
    """START_WORKFLOW -> start_workflow"""
    return screaming_snake.lower()
