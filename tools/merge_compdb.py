#!/usr/bin/env python3
"""把 clang -MJ 写出的编译片段合并成 compile_commands.json。

clang -MJ <file> 每次编译会写一个独立的 JSON 对象（通常带一个结尾逗号），
这个脚本把 <fragment-dir> 下的所有 *.json 读出来，排序后写成标准数组。

用法：
    python3 tools/merge_compdb.py build/compdb compile_commands.json
"""

from __future__ import annotations

import json
import sys
from pathlib import Path


def load_fragment(path: Path) -> dict | None:
    try:
        text = path.read_text(encoding="utf-8").strip()
    except OSError as exc:
        print(f"{path}: {exc}", file=sys.stderr)
        return None

    # clang 在片段后保留逗号，方便构建系统直接拼接
    text = text.rstrip(",").strip()
    if not text:
        return None

    try:
        entry = json.loads(text)
    except json.JSONDecodeError as exc:
        print(f"{path}: invalid JSON: {exc}", file=sys.stderr)
        return None

    if not isinstance(entry, dict) or "file" not in entry:
        print(f"{path}: not a compile_commands entry", file=sys.stderr)
        return None
    return entry


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(f"usage: {argv[0]} <fragment-dir> <output>", file=sys.stderr)
        return 2

    fragment_dir = Path(argv[1])
    output = Path(argv[2])

    entries = []
    for path in sorted(fragment_dir.glob("*.json")):
        entry = load_fragment(path)
        if entry is not None:
            entries.append(entry)

    entries.sort(key=lambda item: item.get("file", ""))

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(entries, stream, indent=2, ensure_ascii=False)
        stream.write("\n")

    print(f"已生成 {output}（{len(entries)} 条）")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
