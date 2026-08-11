#!/usr/bin/env python3
"""CLI entry point: load caipo.protocol.yaml, validate, render both codec targets.

    pip install pyyaml   # or: uvx --with pyyaml python3 protocol/codegen/generate.py
    python3 protocol/codegen/generate.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import yaml

sys.path.insert(0, str(Path(__file__).resolve().parent))

import render_c  # noqa: E402
import render_python  # noqa: E402
from schema import SchemaError, load_protocol  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[2]
SCHEMA_PATH = REPO_ROOT / "protocol" / "caipo.protocol.yaml"
PYTHON_OUTPUT = REPO_ROOT / "software" / "src" / "caipo_pi" / "comm" / "protocol.py"
C_HEADER_OUTPUT = REPO_ROOT / "firmware" / "components" / "caipo_protocol" / "include" / "caipo_protocol.h"
C_SOURCE_OUTPUT = REPO_ROOT / "firmware" / "components" / "caipo_protocol" / "caipo_protocol.c"


def main() -> int:
    raw = yaml.safe_load(SCHEMA_PATH.read_text())
    try:
        protocol = load_protocol(raw)
    except SchemaError as exc:
        print(f"error: {SCHEMA_PATH}: {exc}", file=sys.stderr)
        return 1

    source_rel = SCHEMA_PATH.relative_to(REPO_ROOT).as_posix()

    PYTHON_OUTPUT.write_text(render_python.render(protocol, source_rel))
    C_HEADER_OUTPUT.write_text(render_c.render_header(protocol, source_rel))
    C_SOURCE_OUTPUT.write_text(render_c.render_source(protocol, source_rel))

    for path in (PYTHON_OUTPUT, C_HEADER_OUTPUT, C_SOURCE_OUTPUT):
        print(f"generated {path.relative_to(REPO_ROOT)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
