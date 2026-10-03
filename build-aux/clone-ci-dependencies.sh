#!/usr/bin/env bash
set -euo pipefail

# Keep retries bounded; the calling workflow also limits this step to 10 minutes.
retry() {
    local attempt status delay=5
    for attempt in 1 2 3 4; do
        if "$@"; then
            return 0
        else
            status=$?
        fi
        if [ "$attempt" -eq 4 ]; then
            echo "Dependency download failed after $attempt attempts (exit $status)." >&2
            return "$status"
        fi
        echo "Dependency download attempt $attempt failed; retrying in ${delay}s..." >&2
        sleep "$delay"
        delay=$((delay * 2))
    done
}

clone_once() {
    local status
    if git clone "$1" "$2"; then
        return 0
    else
        status=$?
    fi
    # Only remove a partial clone created by this script, never an existing checkout.
    rm -rf -- "$2"
    return "$status"
}

for directory in i2d-imgui nijilive; do
    if [ -e "$directory" ] || [ -L "$directory" ]; then
        echo "Refusing to overwrite existing dependency: $directory" >&2
        exit 1
    fi
done

retry clone_once https://github.com/inochi2d/i2d-imgui.git i2d-imgui
retry clone_once https://github.com/nijigenerate/nijilive.git nijilive
git -C i2d-imgui checkout c6a78f4a7510fd31a86998b7ceedfc2916ecfae0
# Fetch only the pinned revision's submodules; interrupted updates can be resumed.
retry git -C i2d-imgui submodule update --init --recursive
dub add-local i2d-imgui/ "0.8.0"
dub add-local nijilive/ "0.0.1"
