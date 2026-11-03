# n150 auto-deploy status

> Written by `maple-deploy` on nexus-server (timer, every 5 min). **Do not edit; it's overwritten.**
> Merged `main` of `saijinjun/SoloMapling` (server) and `heavenclient-android` (web client) deploys automatically.
> **HELD** = the change touches ports/relay/Docker/DB wiring and waits for Onyx. **FAILED** = auto-rolled back; fix forward with a new PR.

| | deployed | state |
|---|---|---|
| server | `c577aa7` | held |
| client | `7732617` | held |

## Recent events (newest first)

- 2026-11-03 19:36 · client · **HELD for Onyx review** `7732617..772ee78` - sensitive: web/server.py, web/ws_proxy.py
- 2026-11-03 19:31 · client · **HELD for Onyx review** `7732617..aaebd1c` - sensitive: web/server.py, web/ws_proxy.py
- 2026-11-03 19:31 · server · **HELD for Onyx review** `c577aa7..637c94e` - sensitive: docker-compose.override.yml
- 2026-11-03 19:04 · server · ✅ deployed `c577aa7` - online, 8484 open
- 2026-11-03 17:59 · server · ✅ deployed `706c5b3` - online, 8484 open
- 2026-11-03 01:17 · server · ✅ deployed `fbc2d6b` - online, 8484 open
- 2026-11-02 23:24 · server · ✅ deployed `5f58618` - online, 8484 open
- 2026-10-31 06:47 · server · ⚠ deploy check errored: git fetch -q origin main -> 128: ssh: connect to host github.com port 22: Connection timed out
fatal: Could not read from remote repository.

Please make sure you have the correct access rights
and the repository exists.
- 2026-10-30 22:22 · client · ⚠ deploy check errored: git fetch -q origin main -> 128: git@github.com: Permission denied (publickey).
fatal: Could not read from remote repository.

Please make sure you have the correct access rights
and the repository exists.
- 2026-10-30 22:22 · server · ⚠ deploy check errored: git fetch -q origin main -> 128: git@github.com: Permission denied (publickey).
fatal: Could not read from remote repository.

Please make sure you have the correct access rights
and the repository exists.
- 2026-10-29 18:36 · client · ⚠ deploy check errored: git fetch -q origin main -> 128: ssh: connect to host github.com port 22: Connection timed out
fatal: Could not read from remote repository.

Please make sure you have the correct access rights
and the repository exists.
- 2026-10-29 00:03 · server · ✅ deployed `13531d9` - no runtime change (container not recreated), 8484 open
