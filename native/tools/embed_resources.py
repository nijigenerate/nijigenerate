#!/usr/bin/env python3
"""Embed the resource tree as immutable byte spans, one translation unit per file."""
import argparse
import hashlib
import json
from pathlib import Path


def write_changed(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text(encoding="utf-8") != text:
        path.write_text(text, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    entries = []
    declarations = []
    inventory = []
    for path in sorted(p for p in args.root.rglob("*") if p.is_file()):
        name = path.relative_to(args.root).as_posix()
        key = hashlib.sha256(name.encode("utf-8")).hexdigest()
        data = path.read_bytes()
        symbol = "resource_" + key
        lines = ["#include <cstdint>", "namespace nijigenerate::resources::detail {",
                 f"extern const std::uint8_t {symbol}[] = {{"]
        for i in range(0, len(data), 32):
            lines.append(",".join(str(b) for b in data[i:i + 32]) + ",")
        if not data:
            lines.append("0")
        lines += ["};", "}"]
        write_changed(args.output / (symbol + ".cpp"), "\n".join(lines) + "\n")
        declarations.append(f"extern const std::uint8_t {symbol}[];")
        inventory.append("    {" + json.dumps(name, ensure_ascii=True) +
                         ", {detail::" + symbol + ", " + str(len(data)) + "}}")
        entries.append({"name": name, "size": len(data),
                        "sha256": hashlib.sha256(data).hexdigest()})
    write_changed(args.output / "include/nijigenerate/resources/embedded.hpp", """#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
namespace nijigenerate::resources {
struct EmbeddedResource { std::string_view name; std::span<const std::uint8_t> bytes; };
std::span<const EmbeddedResource> ngEmbeddedResources();
std::optional<std::span<const std::uint8_t>> ngEmbeddedResource(std::string_view name);
}
""")
    source = '#include "nijigenerate/resources/embedded.hpp"\n'
    source += "namespace nijigenerate::resources {\nnamespace detail {\n"
    source += "\n".join(declarations) + "\n}\n"
    source += "static const EmbeddedResource inventory[] = {\n"
    source += ",\n".join(inventory) + "\n};\n"
    source += """std::span<const EmbeddedResource> ngEmbeddedResources() { return inventory; }
std::optional<std::span<const std::uint8_t>> ngEmbeddedResource(std::string_view name) {
    for (const auto& entry : inventory) if (entry.name == name) return entry.bytes;
    return std::nullopt;
}
}
"""
    write_changed(args.output / "embedded.cpp", source)
    write_changed(args.output / "manifest.json", json.dumps(entries, indent=2) + "\n")


if __name__ == "__main__":
    main()
