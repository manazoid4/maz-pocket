"""Dump inbox + library CLI. Works on the files directly: no Core needed.

  python nodinbox.py list [--all] [--project X]
  python nodinbox.py show <id>
  python nodinbox.py claim <id> --agent NAME      (exit 1 if already claimed)
  python nodinbox.py done <id> --agent NAME [--note TEXT]
  python nodinbox.py add "text"
  python nodinbox.py search "words" [--project X] [--limit N]    (library)
  python nodinbox.py import <instagram-export.zip-or-folder>     (library)
  python nodinbox.py stats                                       (library)
Inbox: MAZ_DUMPS_DIR, default ~/nod-inbox. The library lives in <inbox>/library."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

from mazhost import dumps, library
from mazhost.braindump import structure_braindump


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="nodinbox", description=__doc__.splitlines()[0])
    parser.add_argument("--dir", default=os.environ.get("MAZ_DUMPS_DIR") or "~/nod-inbox")
    sub = parser.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("list")
    p.add_argument("--all", action="store_true", help="include done dumps")
    p.add_argument("--project")
    sub.add_parser("show").add_argument("id")
    p = sub.add_parser("claim")
    p.add_argument("id")
    p.add_argument("--agent", required=True)
    p = sub.add_parser("done")
    p.add_argument("id")
    p.add_argument("--agent", required=True)
    p.add_argument("--note", default="")
    sub.add_parser("add").add_argument("text")
    p = sub.add_parser("search")
    p.add_argument("query")
    p.add_argument("--project")
    p.add_argument("--limit", type=int, default=10)
    sub.add_parser("import").add_argument("path")
    sub.add_parser("stats")
    args = parser.parse_args(argv)
    if hasattr(sys.stdout, "reconfigure"):  # captions hold emoji; a cp1252 console must not crash
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    inbox = Path(args.dir).expanduser()
    try:
        if args.cmd == "list":
            for m in dumps.list_dumps(inbox, project=args.project):
                if args.all or m.get("status") != "done":
                    who = f" ({m['claimed_by']})" if m.get("claimed_by") else ""
                    print(f"{m['id']}  [{m['project']}] {m['status']}{who}  {m['summary'][:100]}")
        elif args.cmd == "show":
            print(dumps.get_dump(inbox, args.id)["body"])
        elif args.cmd == "claim":
            dumps.claim_dump(inbox, args.id, args.agent)
            print(f"claimed {args.id} for {args.agent}")
        elif args.cmd == "done":
            dumps.finish_dump(inbox, args.id, args.agent, args.note)
            print(f"done {args.id}")
        elif args.cmd == "search":
            for r in library.search(inbox / "library", args.query, project=args.project, limit=args.limit):
                print(f"{r['id']}  {r['author'] or '-'} · {r['collection'] or '-'} · {r['project']}")
                print(f"  {r['url'] or r['path']}  {r['snippet']}")
        elif args.cmd == "import":
            c = library.import_instagram_export(inbox / "library", Path(args.path).expanduser())
            print(f"found {c['found']}, added {c['added']}, updated {c['updated']}, "
                  f"skipped {c['skipped']}, collections {c['collections']}")
        elif args.cmd == "stats":
            s = library.stats(inbox / "library")
            print(f"{s['total']} items")
            for key in ("kinds", "projects", "collections"):
                print(f"{key}: " + ", ".join(f"{k} {n}" for k, n in sorted(s[key].items(), key=lambda kv: -kv[1])[:20]))
        else:
            structured, _ = structure_braindump("", args.text)
            project = dumps.assign_project(inbox, args.text, structured["summary"])
            meta = dumps.save_dump(inbox, args.text, structured, source="api", project=project)
            print(f"saved {meta['id']} [{meta['project']}]")
    except dumps.DumpError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
