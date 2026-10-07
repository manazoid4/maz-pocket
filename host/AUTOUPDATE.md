# Core auto-update

Core polls GitHub Releases (`manazoid4/maz-pocket`, tags `nod-v{VERSION}-b{build}`) and updates itself with no PC interaction.

## How it works
Every `MAZ_UPDATE_INTERVAL_S` (default 300) Core picks the newest non-draft `nod-v*` release and reads `nod-manifest.json`.

- Firmware: if the release `sha256` differs from the staged `fw/manifest.json`, `nod-fw.bin` is downloaded to a temp file, size and sha256 are verified, then `latest.bin` + `manifest.json` are replaced atomically. The device sees "U: update" on its next poll. A bad download is never staged.
- Core code: if the release `git_sha` differs from the checkout's HEAD, and the checkout has no tracked changes, and no turn/job is in flight, and the sha is an ancestor of `origin/deploy/local`: `git checkout --detach <sha>`, `pip install -r host/requirements.txt` (same python as Core), then `host/restart-core.ps1` is spawned detached to restart the "nod Core" task.
- Rollback: the helper polls `/health` for 90 s expecting `git_sha` = the new sha. If it never appears it checks out the previous sha, reinstalls requirements, restarts, and writes a note to `logs/selfupdate.log`. The bad sha is added to `logs/selfupdate-bad.txt` and is not retried. If pip fails, the checkout is reverted immediately and no restart happens.
- Non-Windows: code is updated on disk and "restart required" is logged.

One-time setup: run `host\BOOTSTRAP-AUTOUPDATE.cmd [worktree]` on the PC (default `C:\Users\manaz\maz-pocket-deploy`). It updates the checkout, installs requirements, and re-points the "nod Core" task at that checkout (state stays in `%LOCALAPPDATA%\MAZ Core`).

## Env vars (.env, prefix MAZ_)
| var | default | meaning |
|---|---|---|
| MAZ_AUTOUPDATE | 1 | 0 disables the loop (manual checks still work) |
| MAZ_UPDATE_INTERVAL_S | 300 | poll interval, min 30 |
| MAZ_REPO_DIR | auto | git checkout to update (default: the checkout containing mazhost) |
| MAZ_GITHUB_TOKEN | empty | optional, raises GitHub rate limit; never logged |
| MAZ_FW_DIR | host/fw | where firmware is staged |

## From a phone
```
curl -H "Authorization: Bearer $TOKEN" http://PC:8787/core/update
curl -X POST -H "Authorization: Bearer $TOKEN" http://PC:8787/core/update/check
```
`/health` also carries `git_sha` and `update`. Every step logs one `selfupdate:` line to `logs/core.log`.
