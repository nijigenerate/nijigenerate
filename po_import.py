#!/usr/bin/env python3
"""
po_import.py — Import translations from a source .po into a target .po
without touching the target's formatting, comments, or untranslated entries.

Usage:
    python3 po_import.py SOURCE.po TARGET.po [--out OUTPUT.po] [--overwrite]

By default:
  - Only fills in entries where the target msgstr is empty ("").
  - With --overwrite: also replaces non-empty msgstr in target with source value.
  - Output is written to TARGET.po in-place unless --out is given.
  - Fuzzy entries in the source are skipped unless --use-fuzzy is given.
"""

import argparse
import re
import sys
from copy import deepcopy


def parse_po(path: str) -> dict[str, list[str]]:
    """
    Parse a .po file and return a dict mapping each msgid (decoded string)
    to its msgstr lines (list of raw quoted strings as they appear in the file,
    e.g. ['"hello"', '"world"']).

    Also returns fuzzy flag per msgid.
    """
    entries: dict[str, dict] = {}

    with open(path, encoding="utf-8") as f:
        lines = f.readlines()

    i = 0
    n = len(lines)

    while i < n:
        # collect flags for the upcoming entry
        flags: list[str] = []
        while i < n and lines[i].startswith("#"):
            if lines[i].startswith("#,"):
                flags.extend(p.strip() for p in lines[i][2:].split(","))
            i += 1

        # skip blank lines between entries
        while i < n and lines[i].strip() == "":
            i += 1

        if i >= n:
            break

        # expect msgid (possibly multi-line)
        if not lines[i].startswith("msgid "):
            i += 1
            continue

        msgid_lines, i = collect_value(lines, i, "msgid")
        msgstr_lines, i = collect_value(lines, i, "msgstr")

        msgid = decode_po_string(msgid_lines)
        entries[msgid] = {
            "msgstr_lines": msgstr_lines,
            "fuzzy": "fuzzy" in flags,
        }

    return entries


def collect_value(lines: list[str], i: int, keyword: str) -> tuple[list[str], int]:
    """
    Collect the quoted string(s) for a keyword like msgid or msgstr.
    Returns (list_of_raw_quoted_strings, next_index).
    """
    n = len(lines)
    raw: list[str] = []

    if i >= n or not lines[i].startswith(keyword + " "):
        return raw, i

    first = lines[i][len(keyword) + 1:].strip()
    raw.append(first)
    i += 1

    # continuation lines start with '"'
    while i < n and lines[i].startswith('"'):
        raw.append(lines[i].strip())
        i += 1

    return raw, i


def decode_po_string(raw_lines: list[str]) -> str:
    """Decode a list of raw quoted PO strings into a Python string."""
    result = ""
    for part in raw_lines:
        if part.startswith('"') and part.endswith('"'):
            inner = part[1:-1]
            # basic unescape
            inner = inner.replace("\\n", "\n").replace("\\t", "\t").replace('\\"', '"').replace("\\\\", "\\")
            result += inner
    return result


def rewrite_target(target_path: str, out_path: str, source_entries: dict,
                   overwrite: bool, use_fuzzy: bool) -> tuple[int, int]:
    """
    Read target .po line-by-line and replace msgstr blocks where appropriate.
    Returns (filled_count, skipped_count).
    """
    with open(target_path, encoding="utf-8") as f:
        lines = f.readlines()

    result: list[str] = []
    i = 0
    n = len(lines)
    filled = 0
    skipped = 0

    while i < n:
        line = lines[i]

        # pass-through comment / blank lines until we hit a msgid
        if not line.startswith("msgid "):
            result.append(line)
            i += 1
            continue

        # --- found a msgid block ---
        # collect the raw msgid lines (including continuations)
        msgid_raw_lines: list[str] = []
        msgid_start = i
        first = line[len("msgid "):].strip()
        msgid_raw_lines.append(first)
        i += 1
        while i < n and lines[i].startswith('"'):
            msgid_raw_lines.append(lines[i].strip())
            i += 1

        msgid = decode_po_string(msgid_raw_lines)

        # emit the msgid lines unchanged
        for raw in [lines[msgid_start]] + lines[msgid_start + 1:i]:
            result.append(raw)

        # now expect msgstr (skip intermediate blank / comment lines — unusual but safe)
        while i < n and lines[i].strip() == "":
            result.append(lines[i])
            i += 1

        if i >= n or not lines[i].startswith("msgstr "):
            # no msgstr found — emit whatever is there and continue
            continue

        # collect original msgstr lines
        msgstr_start = i
        msgstr_lines_orig: list[str] = [lines[i]]
        i += 1
        while i < n and lines[i].startswith('"'):
            msgstr_lines_orig.append(lines[i])
            i += 1

        current_str = decode_po_string([l.strip() for l in msgstr_lines_orig[0:1]] +
                                       [l.strip() for l in msgstr_lines_orig[1:]])
        # re-decode properly
        raw_collected = []
        raw_collected.append(msgstr_lines_orig[0][len("msgstr "):].strip())
        for l in msgstr_lines_orig[1:]:
            raw_collected.append(l.strip())
        current_str = decode_po_string(raw_collected)

        src = source_entries.get(msgid)
        should_replace = False

        if src is not None:
            if src["fuzzy"] and not use_fuzzy:
                pass  # skip fuzzy source
            else:
                src_str = decode_po_string(src["msgstr_lines"])
                if src_str == "":
                    pass  # source is empty — nothing to import
                elif current_str == "":
                    should_replace = True
                elif overwrite and current_str != src_str:
                    should_replace = True

        if should_replace:
            # Build replacement msgstr lines from source, preserving target indentation style
            indent = ""
            new_lines = build_msgstr_lines(src["msgstr_lines"], indent)
            result.extend(new_lines)
            filled += 1
        else:
            # keep original msgstr block unchanged
            result.extend(msgstr_lines_orig)
            if src is not None and current_str == "" and (src is None or decode_po_string(src["msgstr_lines"]) == ""):
                pass
            elif src is not None and should_replace is False and decode_po_string(src["msgstr_lines"]) != "":
                skipped += 1

    # detect line endings from original
    with open(target_path, "rb") as f:
        raw_bytes = f.read()
    crlf = b"\r\n" in raw_bytes

    out_text = "".join(result)
    with open(out_path, "w", encoding="utf-8", newline="\r\n" if crlf else "\n") as f:
        f.write(out_text)

    return filled, skipped


def build_msgstr_lines(src_raw_lines: list[str], indent: str) -> list[str]:
    """Reconstruct msgstr line(s) from source raw quoted strings."""
    if not src_raw_lines:
        return [f'msgstr ""\n']

    first = src_raw_lines[0]
    out = [f"msgstr {first}\n"]
    for extra in src_raw_lines[1:]:
        out.append(f"{extra}\n")
    return out


def main():
    parser = argparse.ArgumentParser(description="Import translations into a .po file without changing its layout.")
    parser.add_argument("source", help="Source .po file (provides translations)")
    parser.add_argument("target", help="Target .po file (receives translations)")
    parser.add_argument("--out", help="Output path (default: overwrite target)")
    parser.add_argument("--overwrite", action="store_true",
                        help="Replace non-empty msgstr in target if source differs")
    parser.add_argument("--use-fuzzy", action="store_true",
                        help="Also import fuzzy entries from source")
    args = parser.parse_args()

    out_path = args.out or args.target

    print(f"Parsing source: {args.source}")
    source_entries = parse_po(args.source)
    print(f"  {len(source_entries)} entries found")

    print(f"Patching target: {args.target} -> {out_path}")
    filled, skipped = rewrite_target(args.target, out_path, source_entries,
                                     overwrite=args.overwrite,
                                     use_fuzzy=args.use_fuzzy)
    print(f"Done. Filled: {filled}, Skipped (already translated): {skipped}")


if __name__ == "__main__":
    main()
