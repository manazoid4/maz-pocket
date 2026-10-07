"""Build the release contract manifest: nod_release_manifest.py <in manifest.json> <out> <tag> <core_version>

Env: RUN_NUMBER, SHA (full commit sha). version = VERSION file; sha/sha256/size come from fw_manifest.py.
"""
import json, os, sys
from pathlib import Path

src, out, tag, core_version = sys.argv[1:5]
m = json.loads(Path(src).read_text(encoding="utf-8"))
m["version"] = (Path(__file__).resolve().parents[1] / "VERSION").read_text(encoding="utf-8").strip()
m["build"] = int(os.environ["RUN_NUMBER"])
m["git_sha"] = os.environ["SHA"]
m["core_version"] = core_version
m["tag"] = tag
Path(out).write_text(json.dumps(m, indent=2), encoding="utf-8")
print(json.dumps(m, indent=2))
