#!/usr/bin/env bash
# Claude Code on the web: install build/test tooling (idempotent, quiet).
[ "$CLAUDE_CODE_REMOTE" = "true" ] || exit 0
cd "$(dirname "$0")/.." || exit 0
python3 -m pip install -q platformio pytest -r host/requirements.txt >/dev/null 2>&1 || echo "cloud-session-setup: pip install failed" >&2
exit 0
