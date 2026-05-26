#!/usr/bin/env python3
"""Strict computer-use regression driver for nijigenerate.

This script is intentionally conservative: a scenario only passes when it has
an explicit probe implementation. Unsupported scenarios fail with exit code 125
so the regression runner cannot accidentally treat a missing UI automation as
success.
"""

from __future__ import annotations

import json
import os
import platform
import subprocess
import sys
import time
from pathlib import Path


INTERACTION_SCENARIOS: dict[str, object] = {}


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def default_test_binary(root: Path) -> Path:
    return root / "out" / "nijigenerate-regression-tests"


def default_app_binary(root: Path) -> Path:
    return root / "out" / "nijigenerate.app" / "Contents" / "MacOS" / "nijigenerate"


def default_app_bundle(root: Path) -> Path:
    return root / "out" / "nijigenerate.app"


def load_manifest(root: Path) -> dict[str, dict[str, str]]:
    binary = Path(os.environ.get("NIJIGENERATE_REGRESSION_TESTS", default_test_binary(root)))
    result = subprocess.run(
        [str(binary), "--computer-use-manifest"],
        cwd=root,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stdout)

    manifest: dict[str, dict[str, str]] = {}
    for line in result.stdout.splitlines():
        if not line.strip():
            continue
        row = json.loads(line)
        manifest[row["id"]] = row
    return manifest


def is_macos() -> bool:
    return platform.system() == "Darwin"


def run_osascript(script: str, timeout: float = 5.0) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["osascript", "-e", script],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
        check=False,
    )


def wait_for_accessibility_process(name: str, timeout: float) -> bool:
    deadline = time.monotonic() + timeout
    script = (
        'tell application "System Events" to '
        f'get name of processes whose name is "{name}"'
    )
    while time.monotonic() < deadline:
        try:
            result = run_osascript(script, timeout=2.0)
        except subprocess.TimeoutExpired:
            result = None
        if result is not None and result.returncode == 0 and name in result.stdout:
            return True
        time.sleep(0.1)
    return False


def has_windowserver_window(owner_name: str) -> bool:
    script = r'''
import CoreGraphics

let windows = CGWindowListCopyWindowInfo([.optionOnScreenOnly, .excludeDesktopElements], kCGNullWindowID) as? [[String: Any]] ?? []
for window in windows {
    let owner = window[kCGWindowOwnerName as String] as? String ?? ""
    let layer = window[kCGWindowLayer as String] as? Int ?? -1
    let alpha = window[kCGWindowAlpha as String] as? Double ?? 0.0
    if owner == "OWNER_NAME" && layer == 0 && alpha > 0.0 {
        print("visible")
        exit(0)
    }
}
exit(1)
'''.replace("OWNER_NAME", owner_name.replace("\\", "\\\\").replace('"', '\\"'))
    try:
        result = subprocess.run(
            ["swift", "-e", script],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=5.0,
            check=False,
        )
    except subprocess.TimeoutExpired:
        return False
    return result.returncode == 0 and "visible" in result.stdout


def wait_for_windowserver_window(owner_name: str, timeout: float) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if has_windowserver_window(owner_name):
            return True
        time.sleep(0.2)
    return False


def run_gui_agent_probe(root: Path, scenario_id: str, log_path: Path) -> int:
    """Launch through LaunchServices and verify the GUI is actually visible.

    This is deliberately separate from the fast subprocess smoke path: it keeps
    the app alive long enough for an external computer-use agent to attach, and
    requires a WindowServer window so process-only launches are not counted as
    computer-use coverage.
    """
    app = Path(os.environ.get("NIJIGENERATE_APP", default_app_binary(root)))
    if not app.exists():
        print(f"app binary does not exist: {app}", file=sys.stderr)
        print("build it first with: dub build -c osx-full", file=sys.stderr)
        return 2

    frames = max(int(os.environ.get("NIJIGENERATE_REGRESSION_FRAMES", "120")), 90)
    frame_delay_ms = int(os.environ.get("NIJIGENERATE_REGRESSION_FRAME_DELAY_MS", "10"))
    command = [
        str(app),
        "--regression-smoke",
        scenario_id,
        "--regression-computer-use",
        "--regression-frames",
        str(frames),
        "--regression-frame-delay-ms",
        str(frame_delay_ms),
    ]

    with log_path.open("w", encoding="utf-8") as log:
        process = subprocess.Popen(
            command,
            cwd=root,
            text=True,
            stdout=log,
            stderr=subprocess.STDOUT,
        )
        try:
            if not wait_for_accessibility_process("nijigenerate", timeout=5.0):
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                print(f"computer-use agent could not observe nijigenerate process for {scenario_id}", file=sys.stderr)
                return 3
            if not wait_for_windowserver_window("nijigenerate", timeout=10.0):
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                print(f"computer-use agent could not observe a nijigenerate window for {scenario_id}", file=sys.stderr)
                return 4
            return_code = process.wait(timeout=float(os.environ.get("NIJIGENERATE_REGRESSION_TIMEOUT", "45")))
        except subprocess.TimeoutExpired:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
            print(f"computer-use app timed out for {scenario_id}; see {log_path}", file=sys.stderr)
            return 124

    if return_code != 0:
        print(f"computer-use app failed for {scenario_id}; see {log_path}", file=sys.stderr)
        return return_code
    return 0


def run_app_smoke(root: Path, scenario_id: str) -> int:
    app = Path(os.environ.get("NIJIGENERATE_APP", default_app_binary(root)))
    if not app.exists():
        print(f"app binary does not exist: {app}", file=sys.stderr)
        print("build it first with: dub build -c osx-full", file=sys.stderr)
        return 2

    artifact_dir = root / "out" / "regression-computer-use"
    artifact_dir.mkdir(parents=True, exist_ok=True)
    log_path = artifact_dir / f"{scenario_id}.log"

    use_gui_agent = os.environ.get("NIJIGENERATE_COMPUTER_USE_AGENT", "1") != "0" and is_macos()
    if use_gui_agent:
        result_code = run_gui_agent_probe(root, scenario_id, log_path)
    else:
        command = [
            str(app),
            "--regression-smoke",
            scenario_id,
            "--regression-frames",
            os.environ.get("NIJIGENERATE_REGRESSION_FRAMES", "8"),
            "--regression-frame-delay-ms",
            os.environ.get("NIJIGENERATE_REGRESSION_FRAME_DELAY_MS", "0"),
        ]
        with log_path.open("w", encoding="utf-8") as log:
            result = subprocess.run(
                command,
                cwd=root,
                text=True,
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=float(os.environ.get("NIJIGENERATE_REGRESSION_TIMEOUT", "30")),
                check=False,
            )
        result_code = result.returncode

    if result_code != 0:
        print(f"app-smoke failed for {scenario_id}; see {log_path}", file=sys.stderr)
        return result_code

    print(f"app-smoke OK: {scenario_id} ({log_path})")
    return 0


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: computer_use_driver.py <scenario-id>", file=sys.stderr)
        return 2

    scenario_id = argv[1]
    root = repo_root()
    manifest = load_manifest(root)

    if scenario_id not in manifest:
        print(f"unknown computer-use scenario: {scenario_id}", file=sys.stderr)
        return 2

    if scenario_id in INTERACTION_SCENARIOS:
        return run_app_smoke(root, scenario_id)

    row = manifest[scenario_id]
    print(
        "computer-use interaction is not implemented: "
        f"{scenario_id} ({row['category']} - {row['title']}). "
        "A visible-window smoke check is not a computer-use test; add explicit "
        "observe/click/type/drag assertions before enabling this scenario.",
        file=sys.stderr,
    )
    return 125


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
