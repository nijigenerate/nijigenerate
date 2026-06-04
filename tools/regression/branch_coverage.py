#!/usr/bin/env python3
"""Run regression tests with LDC profile counters and enforce branch/block coverage.

This is intentionally stricter than scenario accounting. It builds the regression
runner with frontend PGO counters, runs the full automated suite, and fails when
any reported function or internal block counter remains zero.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


FUNCTION_RE = re.compile(r"^  (?P<name>.+):$")
FUNCTION_COUNT_RE = re.compile(r"^    Function count: (?P<count>\d+)")
BLOCK_COUNTS_RE = re.compile(r"^    Block counts: \[(?P<counts>.*)\]")


def run(cmd: list[str], cwd: Path, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=cwd, env=env, check=True)


def should_count_function(name: str, include_prefixes: tuple[str, ...]) -> bool:
    symbol = name.split(":", 1)[-1]
    return any(symbol.startswith(prefix) for prefix in include_prefixes)


def parse_profdata_show(text: str, include_prefixes: tuple[str, ...]) -> tuple[int, int, list[str]]:
    current_function: str | None = None
    current_function_counted = False
    total_counters = 0
    total_functions = 0
    failures: list[str] = []

    for line in text.splitlines():
        match = FUNCTION_RE.match(line)
        if match:
            current_function = match.group("name")
            current_function_counted = should_count_function(current_function, include_prefixes)
            if current_function_counted:
                total_functions += 1
            continue

        match = FUNCTION_COUNT_RE.match(line)
        if match and current_function and current_function_counted:
            total_counters += 1
            if int(match.group("count")) == 0:
                failures.append(f"{current_function}: function count is zero")
            continue

        match = BLOCK_COUNTS_RE.match(line)
        if match and current_function and current_function_counted:
            raw_counts = match.group("counts").strip()
            if not raw_counts:
                continue
            counts = [int(value.strip()) for value in raw_counts.split(",")]
            total_counters += len(counts)
            for index, count in enumerate(counts):
                if count == 0:
                    failures.append(f"{current_function}: block counter {index} is zero")

    return total_functions, total_counters, failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--tmp", type=Path, default=Path("/private/tmp/nijigenerate-branch-coverage"))
    parser.add_argument(
        "--include-prefix",
        action="append",
        default=["_D12nijigenerate"],
        help="Mangled function-name prefix to include. Defaults to nijigenerate production code.",
    )
    args = parser.parse_args()

    repo = args.repo.resolve()
    tmp = args.tmp.resolve()
    profraw_dir = tmp / "profraw"
    profdata = tmp / "merged.profdata"

    llvm_profdata = shutil.which("llvm-profdata")
    if llvm_profdata is None:
        print("llvm-profdata is required for branch coverage", file=sys.stderr)
        return 2

    if tmp.exists():
        shutil.rmtree(tmp)
    profraw_dir.mkdir(parents=True)

    run(["dub", "build", "-q", "-c", "regression-branch-coverage"], cwd=repo)

    env = os.environ.copy()
    env["LLVM_PROFILE_FILE"] = str(profraw_dir / "regression-%p.profraw")
    run(["./out/nijigenerate-regression-tests"], cwd=repo, env=env)

    profraws = sorted(profraw_dir.glob("*.profraw"))
    if not profraws:
        print("no .profraw files were produced", file=sys.stderr)
        return 2

    run([llvm_profdata, "merge", "-sparse", *map(str, profraws), "-o", str(profdata)], cwd=repo)
    shown = subprocess.check_output(
        [llvm_profdata, "show", "--counts", "--all-functions", str(profdata)],
        cwd=repo,
        text=True,
    )
    include_prefixes = tuple(args.include_prefix)
    total_functions, total_counters, failures = parse_profdata_show(shown, include_prefixes)
    print("branch-coverage-include-prefixes:", ",".join(include_prefixes))
    print(f"branch-coverage-functions: {total_functions}")
    print(f"branch-coverage-counters: {total_counters}")
    print(f"branch-coverage-uncovered: {len(failures)}")

    if failures:
        for failure in failures[:200]:
            print(failure, file=sys.stderr)
        if len(failures) > 200:
            print(f"... {len(failures) - 200} more uncovered counters", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
