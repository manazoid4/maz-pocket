#!/usr/bin/env bash
# Post a fake Claude Code approval to Core so you can test the Cardputer overlay.
# Usage: host/hooks/test-approval.sh ["command text"] [Tool] [project]
# Token: $MAZ_TOKEN, else MAZ_TOKEN= in host/.env (never printed). Core: $MAZ_CORE_URL or http://127.0.0.1:8787
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
summary="${1:-rm -rf node_modules && npm install --no-audit}"
tool="${2:-Bash}"
project="${3:-maz-pocket}"
base="${MAZ_CORE_URL:-http://127.0.0.1:8787}"
token="${MAZ_TOKEN:-}"
if [ -z "$token" ]; then
  for f in "$here/../.env" ".env"; do
    [ -f "$f" ] && token="$(grep -E '^MAZ_TOKEN=' "$f" | head -1 | cut -d= -f2- | tr -d '\r"'"'"'')" && [ -n "$token" ] && break
  done
fi
[ -n "$token" ] || { echo "No MAZ_TOKEN (env or host/.env)" >&2; exit 2; }
hdr=(-H "Authorization: Bearer $token" -H "Content-Type: application/json")
body="$(python3 - "$tool" "$summary" "$project" <<'PY'
import json, sys
print(json.dumps({"tool": sys.argv[1], "summary": sys.argv[2], "project": sys.argv[3], "session_id": "test-approval"}))
PY
)"
id="$(curl -fsS "${hdr[@]}" -d "$body" "$base/buddy/request" | python3 -c 'import json,sys;print(json.load(sys.stdin)["id"])')"
echo "Request $id sent. Press Y/ENTER, N/ESC or A on the Cardputer (60 s)..."
for _ in $(seq 1 15); do
  d="$(curl -fsS "${hdr[@]}" "$base/buddy/state?id=$id&wait=4" | python3 -c 'import json,sys;print(json.load(sys.stdin).get("decision") or "")')"
  if [ -n "$d" ]; then echo "Decision: $d"; exit 0; fi
done
curl -fsS "${hdr[@]}" -d "{\"id\":\"$id\",\"decision\":\"cancel\"}" "$base/buddy/decide" >/dev/null || true
echo "Timed out (no decision); cancelled."
