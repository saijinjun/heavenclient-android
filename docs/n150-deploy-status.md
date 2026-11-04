# n150 auto-deploy status

> Written by `maple-deploy` on nexus-server (timer, every 5 min). **Do not edit; it's overwritten.**
> Merged `main` of `saijinjun/SoloMapling` (server) and `heavenclient-android` (web client) deploys automatically.
> **HELD** = the change touches ports/relay/Docker/DB wiring and waits for Onyx. **FAILED** = auto-rolled back; fix forward with a new PR.

| | deployed | state |
|---|---|---|
| server | `e70652f` | ok |
| client | `1889e53` | ok |

## Recent events (newest first)

- 2026-11-04 23:45 · client · ✅ deployed `1889e53` - wasm rebuilt; page+wasm 200, proxy->8484 handshake ok
- 2026-11-04 22:14 · client · ✅ deployed `03a62e6` - wasm rebuilt; page+wasm 200, proxy->8484 handshake ok
- 2026-11-04 18:26 · client · ✅ deployed `c5ac379` - wasm rebuilt; page+wasm 200, proxy->8484 handshake ok
- 2026-11-04 17:40 · server · ⚠ deploy check errored: git fetch -q origin main -> 128: git@github.com: Permission denied (publickey).
fatal: Could not read from remote repository.

Please make sure you have the correct access rights
and the repository exists.
- 2026-11-04 16:21 · client · ✅ deployed `0956b35` - wasm rebuilt; page+wasm 200, proxy->8484 handshake ok
- 2026-11-04 14:18 · client · ✅ deployed `7d197ab` - wasm rebuilt; page+wasm 200, proxy->8484 handshake ok
- 2026-11-04 13:15 · client · ✅ deployed `40128bb` - wasm rebuilt; page+wasm 200, proxy->8484 handshake ok
- 2026-11-04 12:38 · client · ✅ deployed `4238243` - wasm rebuilt; page+wasm 200, proxy->8484 handshake ok
- 2026-11-04 12:38 · server · ✅ deployed `e70652f` - online, 8484 open
- 2026-11-04 09:18 · client · ✅ deployed `70d9112` - wasm rebuilt; page+wasm 200, proxy->8484 handshake ok
- 2026-11-04 01:33 · client · ✅ deployed `d374641` - wasm rebuilt, web services restarted; page+wasm 200, proxy->8484 handshake ok
- 2026-11-04 01:31 · client · Onyx approved `d374641` (web/server.py, web/ws_proxy.py)
- 2026-11-03 21:45 · client · **HELD for Onyx review** `7732617..d374641` - sensitive: web/server.py, web/ws_proxy.py
- 2026-11-03 20:16 · server · ✅ deployed `25fa1cd` - online, 8484 open
- 2026-11-03 20:14 · server · Onyx approved `25fa1cd` (docker-compose.override.yml)
- 2026-11-03 20:04 · server · **HELD for Onyx review** `c577aa7..25fa1cd` - sensitive: docker-compose.override.yml
- 2026-11-03 19:36 · client · **HELD for Onyx review** `7732617..772ee78` - sensitive: web/server.py, web/ws_proxy.py
- 2026-11-03 19:31 · client · **HELD for Onyx review** `7732617..aaebd1c` - sensitive: web/server.py, web/ws_proxy.py
- 2026-11-03 19:31 · server · **HELD for Onyx review** `c577aa7..637c94e` - sensitive: docker-compose.override.yml
- 2026-11-03 19:04 · server · ✅ deployed `c577aa7` - online, 8484 open
