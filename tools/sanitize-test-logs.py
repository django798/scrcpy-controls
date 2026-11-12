#!/usr/bin/env python3
# Copyright (C) 2026 moasm
# SPDX-License-Identifier: Apache-2.0
"""Remove inherited environment values before retaining/sharing Meson logs."""
import json
import sys
from pathlib import Path


def sanitize(directory: Path) -> None:
    for path in directory.glob("testlog*.txt"):
        lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
        path.write_text("".join(line for line in lines
                                if not line.startswith("Inherited environment:")),
                        encoding="utf-8")
    for path in directory.glob("testlog*.json"):
        records = []
        for line in path.read_text(encoding="utf-8").splitlines():
            record = json.loads(line)
            record.pop("env", None)
            records.append(json.dumps(record, ensure_ascii=False))
        path.write_text("\n".join(records) + "\n", encoding="utf-8")


if __name__ == "__main__":
    for name in sys.argv[1:]:
        sanitize(Path(name))
