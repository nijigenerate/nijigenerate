#!/usr/bin/env python3
"""Extract regression scenarios and production branch sites.

This is an audit tool, not a behavioral test. It supports doc/testing.md by
making the current scenario catalog and branch-shaped input variations visible
so runtime tests can be planned from evidence.
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


SCENARIO_RE = re.compile(
    r'Scenario\(\s*"(?P<id>[^"]+)"\s*,\s*"(?P<category>[^"]+)"\s*,\s*"(?P<title>[^"]*)"\s*,\s*(?P<status>\w+)\s*,\s*"(?P<note>[^"]*)"',
    re.MULTILINE | re.DOTALL,
)
CASE_RE = re.compile(r'case\s+"(?P<id>[^"]+)"\s*:')
RUNCASE_RE = re.compile(r'runCase\(\s*"(?P<name>[^"]+)"\s*,\s*&(?P<fn>[A-Za-z_][A-Za-z0-9_]*)')
FUNCTION_RE = re.compile(r'\b(?:public\s+)?(?:private\s+)?void\s+(?P<fn>test[A-Za-z0-9_]*)\s*\(')


BRANCH_PATTERNS = [
    ("if", re.compile(r"\bif\s*\(")),
    ("else", re.compile(r"\belse\b")),
    ("switch", re.compile(r"\bswitch\s*\(")),
    ("case", re.compile(r"\bcase\b")),
    ("default", re.compile(r"\bdefault\s*:")),
    ("cast", re.compile(r"\bcast\s*\(")),
    ("null", re.compile(r"\bis\s+null\b|!\s*is\s+null\b")),
    ("enforce", re.compile(r"\benforce\s*\(")),
    ("throw", re.compile(r"\bthrow\s+new\b|\bthrow\s*\(")),
    ("catch", re.compile(r"\bcatch\s*\(")),
]

INVALID_BEHAVIORAL_MARKERS = (
    "contract",
    "coverage",
    "inventory",
    "runnable",
    "source-contract",
    "source scan",
    "source-derived",
    "accounting guard",
)


@dataclass(frozen=True)
class Scenario:
    id: str
    category: str
    title: str
    status: str
    note: str
    file: str
    line: int
    runner_functions: tuple[str, ...]
    behavioral_status: str


@dataclass(frozen=True)
class BranchSite:
    file: str
    line: int
    kind: str
    text: str


def line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def iter_d_files(root: Path, rel_root: str) -> Iterable[Path]:
    base = root / rel_root
    if not base.exists():
        return []
    return sorted(path for path in base.rglob("*.d") if path.is_file())


def extract_runner_map(regression_source: str) -> dict[str, set[str]]:
    runner_map: dict[str, set[str]] = {}
    active_ids: list[str] = []
    for line in regression_source.splitlines():
        case_match = CASE_RE.search(line)
        if case_match:
            active_ids.append(case_match.group("id"))
            continue
        run_match = RUNCASE_RE.search(line)
        if run_match and active_ids:
            for scenario_id in active_ids:
                runner_map.setdefault(scenario_id, set()).add(run_match.group("fn"))
            active_ids = []
            continue
        if active_ids and line.strip().startswith(("return ", "default:")):
            active_ids = []
    return runner_map


def behavioral_status(scenario: Scenario, runner_functions: tuple[str, ...]) -> str:
    haystack = " ".join(
        [scenario.id, scenario.category, scenario.title, scenario.note, *runner_functions]
    ).lower()
    if scenario.status in {"computer-use", "computerUse"}:
        return "computer-use"
    if scenario.status != "automated":
        return "not-automated"
    if scenario.id.startswith("audit.") or scenario.category == "Regression Audit":
        return "audit-only"
    if any(marker in haystack for marker in INVALID_BEHAVIORAL_MARKERS):
        return "suspect-audit"
    if not runner_functions:
        return "missing-runner"
    return "runtime-candidate"


def extract_scenarios(root: Path) -> list[Scenario]:
    regression_path = root / "source" / "nijigenerate_tests" / "regression.d"
    regression_source = regression_path.read_text(encoding="utf-8")
    runner_map = extract_runner_map(regression_source)

    scenarios: list[Scenario] = []
    for path in iter_d_files(root, "source/nijigenerate_tests"):
        source = path.read_text(encoding="utf-8", errors="replace")
        rel = path.relative_to(root).as_posix()
        for match in SCENARIO_RE.finditer(source):
            scenario_id = match.group("id")
            runner_functions = tuple(sorted(runner_map.get(scenario_id, ())))
            scenario = Scenario(
                id=scenario_id,
                category=match.group("category"),
                title=" ".join(match.group("title").split()),
                status=match.group("status"),
                note=" ".join(match.group("note").split()),
                file=rel,
                line=line_number(source, match.start()),
                runner_functions=runner_functions,
                behavioral_status="",
            )
            scenarios.append(
                Scenario(
                    **{
                        **asdict(scenario),
                        "behavioral_status": behavioral_status(scenario, runner_functions),
                    }
                )
            )
    primary_by_id: dict[str, Scenario] = {}
    for scenario in scenarios:
        current = primary_by_id.get(scenario.id)
        if current is None or scenario_priority(scenario) < scenario_priority(current):
            primary_by_id[scenario.id] = scenario
    return sorted(primary_by_id.values(), key=lambda s: s.id)


def scenario_priority(scenario: Scenario) -> tuple[int, int, str, int]:
    has_runner = 0 if scenario.runner_functions else 1
    in_regression = 0 if scenario.file == "source/nijigenerate_tests/regression.d" else 1
    return (has_runner, in_regression, scenario.file, scenario.line)


def extract_test_functions(root: Path) -> list[dict[str, object]]:
    result: list[dict[str, object]] = []
    for path in iter_d_files(root, "source/nijigenerate_tests"):
        source = path.read_text(encoding="utf-8", errors="replace")
        rel = path.relative_to(root).as_posix()
        for match in FUNCTION_RE.finditer(source):
            fn = match.group("fn")
            result.append({"function": fn, "file": rel, "line": line_number(source, match.start())})
    return sorted(result, key=lambda item: (str(item["function"]), str(item["file"])))


def extract_branch_sites(root: Path) -> list[BranchSite]:
    sites: list[BranchSite] = []
    for path in iter_d_files(root, "source/nijigenerate"):
        source = path.read_text(encoding="utf-8", errors="replace")
        rel = path.relative_to(root).as_posix()
        for line_index, line in enumerate(source.splitlines(), start=1):
            stripped = line.strip()
            if not stripped or stripped.startswith("//"):
                continue
            for kind, pattern in BRANCH_PATTERNS:
                if pattern.search(stripped):
                    sites.append(BranchSite(rel, line_index, kind, stripped[:220]))
    return sites


def summarize(scenarios: list[Scenario], tests: list[dict[str, object]], branches: list[BranchSite]) -> dict[str, object]:
    scenario_status: dict[str, int] = {}
    behavioral_status: dict[str, int] = {}
    branch_kinds: dict[str, int] = {}
    for scenario in scenarios:
        scenario_status[scenario.status] = scenario_status.get(scenario.status, 0) + 1
        behavioral_status[scenario.behavioral_status] = behavioral_status.get(scenario.behavioral_status, 0) + 1
    for branch in branches:
        branch_kinds[branch.kind] = branch_kinds.get(branch.kind, 0) + 1
    return {
        "scenario_count": len(scenarios),
        "test_function_count": len(tests),
        "branch_site_count": len(branches),
        "scenario_status": dict(sorted(scenario_status.items())),
        "behavioral_status": dict(sorted(behavioral_status.items())),
        "branch_kinds": dict(sorted(branch_kinds.items())),
    }


def render_markdown(data: dict[str, object]) -> str:
    summary = data["summary"]
    lines = [
        "# Regression Scenario Extraction",
        "",
        "This file is generated by `tools/regression/extract_test_scenarios.py`.",
        "It is an audit artifact; it does not count as behavioral audit.",
        "",
        "## Summary",
        "",
    ]
    assert isinstance(summary, dict)
    for key in ("scenario_count", "test_function_count", "branch_site_count"):
        lines.append(f"- `{key}`: {summary[key]}")
    lines += ["", "## Behavioral Scenario Status", ""]
    for key, value in dict(summary["behavioral_status"]).items():
        lines.append(f"- `{key}`: {value}")
    lines += ["", "## Production Branch Site Kinds", ""]
    for key, value in dict(summary["branch_kinds"]).items():
        lines.append(f"- `{key}`: {value}")
    lines += ["", "## Non-Runtime Or Suspect Scenarios", ""]
    scenarios = data["scenarios"]
    assert isinstance(scenarios, list)
    for scenario in scenarios:
        assert isinstance(scenario, dict)
        if scenario["behavioral_status"] == "runtime-candidate":
            continue
        lines.append(
            f"- `{scenario['id']}`: `{scenario['behavioral_status']}` "
            f"at `{scenario['file']}:{scenario['line']}`"
        )
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default=".", help="repository root")
    parser.add_argument("--format", choices=("json", "markdown"), default="json")
    parser.add_argument("--output", help="write output to this path")
    args = parser.parse_args()

    root = Path(args.repo).resolve()
    scenarios = extract_scenarios(root)
    tests = extract_test_functions(root)
    branches = extract_branch_sites(root)
    data = {
        "summary": summarize(scenarios, tests, branches),
        "scenarios": [asdict(scenario) for scenario in scenarios],
        "test_functions": tests,
        "branch_sites": [asdict(branch) for branch in branches],
    }
    if args.format == "json":
        rendered = json.dumps(data, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    else:
        rendered = render_markdown(data)

    if args.output:
        Path(args.output).write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
