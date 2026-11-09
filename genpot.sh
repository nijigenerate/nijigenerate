#!/bin/sh
set -eu
cd "$(dirname "$0")"
exec python3 tools/generate-pot.py "$@"
