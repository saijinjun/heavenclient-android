# n150 auto-deploy status

> Written by `maple-deploy` on nexus-server (timer, every 5 min). **Do not edit; it's overwritten.**
> Merged `main` of `saijinjun/SoloMapling` (server) and `heavenclient-android` (web client) deploys automatically.
> **HELD** = the change touches ports/relay/Docker/DB wiring and waits for Onyx. **FAILED** = auto-rolled back; fix forward with a new PR.

| | deployed | state |
|---|---|---|
| server | `235fa63` | held |
| client | `13e961b` | ok |

## Recent events (newest first)

- 2026-11-07 19:20 · server · **HELD for Onyx review** `235fa63..0f6af33` - sensitive: config.yaml (PROXY_PROTOCOL_TRUSTED)
- 2026-11-07 18:08 · client · ✅ deployed `13e961b` - wasm rebuilt; page+wasm 200, proxy->8484 handshake ok
- 2026-11-07 16:17 · client · ✅ deployed `688545a` - wasm rebuilt; page+wasm 200, proxy->8484 handshake ok
- 2026-11-07 16:17 · server · **HELD for Onyx review** `235fa63..281ebb9` - sensitive: config.yaml (PROXY_PROTOCOL_TRUSTED)
- 2026-11-07 16:00 · client · ✅ deployed `f4af705` - wasm rebuilt; page+wasm 200, proxy->8484 handshake ok
- 2026-11-07 15:12 · server · **HELD for Onyx review** `235fa63..b2689cb` - sensitive: config.yaml (PROXY_PROTOCOL_TRUSTED)
- 2026-11-07 15:06 · client · ✅ deployed `55625b3` - wasm rebuilt; page+wasm 200, proxy->8484 handshake ok
- 2026-11-07 15:01 · client · ✅ deployed `0040c6d` - no runtime change (docs/android/tests only)
- 2026-11-07 14:40 · server · ✅ deployed `235fa63` - online, 8484 open
- 2026-11-07 04:48 · client · ⚠ deploy check errored: git fetch -q origin main -> 128: ssh: connect to host github.com port 22: No route to host
fatal: Could not read from remote repository.

Please make sure you have the correct access rights
and the repository exists.
- 2026-11-07 04:17 · client · ⚠ deploy check errored: git fetch -q origin main -> 128: ssh: connect to host github.com port 22: Connection timed out
fatal: Could not read from remote repository.

Please make sure you have the correct access rights
and the repository exists.
- 2026-11-07 02:59 · client · ✅ deployed `9668495` - wasm rebuilt; page+wasm 200, proxy->8484 handshake ok
- 2026-11-07 02:30 · server · ✅ deployed `2a81c6b` - online, 8484 open
- 2026-11-07 02:23 · server · ✅ deployed `421ec4c` - online, 8484 open
- 2026-11-07 01:48 · server · ✅ deployed `89841db` - online, 8484 open
- 2026-11-07 01:24 · server · ✅ deployed `3fcdb7f` - online, 8484 open
- 2026-11-06 15:03 · server · ⚠ deploy check errored: git fetch -q origin main -> 128: ssh: connect to host github.com port 22: Connection timed out
fatal: Could not read from remote repository.

Please make sure you have the correct access rights
and the repository exists.
- 2026-11-06 10:57 · server · ✅ deployed `8cc741a` - online, 8484 open
- 2026-11-06 10:44 · client · ✅ deployed `fc759a3` - wasm rebuilt; page+wasm 200, proxy->8484 handshake ok
- 2026-11-06 01:19 · client · ✅ deployed `3bac3b9` - wasm rebuilt; page+wasm 200, proxy->8484 handshake ok
