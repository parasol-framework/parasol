#!/usr/bin/env python3
"""Generate RFC 5892 context property ranges from matching Unicode UCD files."""

import argparse
from pathlib import Path


def ranges(path, values):
    entries = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        code, value = line.split("#", 1)[0].split(";", 1)
        value = value.strip()
        if value not in values:
            continue
        points = code.strip().split("..")
        first = int(points[0], 16)
        last = int(points[-1], 16)
        entries.append((first, last, values[value]))
    entries.sort()
    merged = []
    for first, last, value in entries:
        if merged and merged[-1][2] == value and merged[-1][1] + 1 == first:
            merged[-1] = (merged[-1][0], last, value)
        else:
            merged.append((first, last, value))
    return merged


def emit(name, entries):
    result = [f"constexpr Range {name}[] = {{"]
    for first, last, value in entries:
        result.append(f"   {{0x{first:06x}, 0x{last:06x}, {value}}},")
    result.append("};")
    return "\n".join(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scripts", type=Path, help="Unicode 16.0.0 Scripts.txt")
    parser.add_argument("joining", type=Path, help="Unicode 16.0.0 extracted/DerivedJoiningType.txt")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    script_values = {"Greek": 1, "Hebrew": 2, "Hiragana": 3, "Katakana": 4, "Han": 5}
    joining_values = {"L": 1, "D": 2, "R": 3, "T": 4}
    scripts = ranges(args.scripts, script_values)
    joining = ranges(args.joining, joining_values)
    header = """// Generated from Unicode 16.0.0 Scripts.txt and DerivedJoiningType.txt.
// Sources: https://www.unicode.org/Public/16.0.0/ucd/Scripts.txt
// https://www.unicode.org/Public/16.0.0/ucd/extracted/DerivedJoiningType.txt
// Unicode data terms: https://www.unicode.org/terms_of_use.html
#pragma once

#include <cstdint>

namespace rtc::precis::ucd {
struct Range { uint32_t first; uint32_t last; uint8_t value; };

"""
    header += emit("scripts", scripts) + "\n\n" + emit("joining_types", joining)
    header += "\n\n}\n"
    args.output.write_text(header, encoding="utf-8")


if __name__ == "__main__":
    main()
