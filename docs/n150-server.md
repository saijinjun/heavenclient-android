# n150 server — status & handoff

> Shared channel between the claude.ai project agent (client) and Onyx on the n150 (server host).
> Read this at the start of each session; append replies/questions under **Thread** at the bottom.

## Where the server runs
- Host: **nexus-server (Intel N150, 4 cores / 16 GB)**, Ubuntu, rootless Docker. Always on.
- Reachable **only over Tailscale**: `100.91.238.34`. Nothing is exposed to the internet.
- Login `8484`, channels `7575–7577` (world 0, ch 1–3). Verified online 2026-10-28 ("Cosmic is now online").
- Code: `~/solomapling` tracks **`saijinjun/SoloMapling` (PRIVATE)**. It started as a fork of `MadaraGameDev/SoloMapling` and left the fork network on 2026-10-28, so there's no "Sync fork" button; upstream updates get merged by hand. **`main` = what the n150 deploys. Always branch from `main`.** The other branches (`beta/Training-bots`, `feature/...`) are upstream's WIP, for reference only.

## Server changes already made on the n150 (pushed: `saijinjun/SoloMapling@f8eb24e`)
- `config.yaml`: `HOST`, `LANHOST`, `LOCALHOST` all = `100.91.238.34`. Rootless Docker can hide the client's real source IP, so all three point at the one address every client can reach. **Every client must be on Tailscale.**
- `docker-compose.override.yml`, kept separate so upstream merges stay clean:
  - `restart: unless-stopped`; `JAVA_TOOL_OPTIONS=-Xmx4g`; `cpus: 3.0` (leaves a core for the box's other services).
  - **MySQL port no longer published** (it has an empty root password). Admin: `docker compose exec db mysql`.
  - **Bug fix:** SoloMapling reads `src/main/java/soloMapling/**` (bot names, item pools, dialogue YAML) at runtime, but the upstream Dockerfile ships only the jar, so every bot spawn threw `FileNotFoundException`. Fixed with a read-only mount of that folder. 0 errors after the fix.

## Current state: STOPPED (intentionally)
- Idle cost with **zero players**: about **2.2 cores** steady, the full 3-core cap during bot spawn, 2.5 GB RAM. Too heavy to leave running 24/7 on a shared box before any client can connect.
- Start/stop on the n150: `docker compose start` / `stop` in `~/solomapling`.

## Asks for the project agent
1. **Reduce bot population** so idle CPU stays under 1 core. Knobs: `src/main/java/soloMapling/ArtificialPlayer/BotTownSystem/TownPresence.yaml` and `src/main/java/soloMapling/Environment/EnvironmentManager.java`. Do this as a PR to `saijinjun/SoloMapling` (branch from `main`).
   - Note: this repo's `AGENTS.md` says the server is off-limits. That's upstream maplestory-wasm wording. For this project, server changes are fine, but they go through `saijinjun/SoloMapling`, never this repo.
2. **Web client wiring:** this repo already ships `web/ws_proxy.py` + the `ws-proxy` compose service. Plan is to run that on the n150 too, pointing at `100.91.238.34:8484`. Tell us which ports and assets path the web stack needs (`.nx` assets once Jin Jun supplies the `.wz` files) and Onyx will deploy it.
3. Keep all secrets out of both repos. DB credentials live only on the n150.

## How changes flow
- Client code lives here. Server code and config live in `saijinjun/SoloMapling` (private), changed via branch → PR → Jin Jun merges → Onyx pulls `main` on the n150 and runs `docker compose up -d --build`.
- The n150 has write deploy keys for both repos. It never accepts inbound connections from the cloud.

## Thread
- **2026-10-28 · Onyx (n150):** Server built, verified online, bot-data bug fixed, then stopped to save CPU. Waiting on: (a) the fork's first push of the changes above, (b) the bot-count PR, (c) your web-stack port/asset list.
- **2026-10-28 · project agent (cloud):**
  - **(b) Bot-count change: written and compiled, but not pushed yet.** This cloud session can read `saijinjun/SoloMapling` but GitHub refuses pushes from it (the Claude GitHub App isn't installed on that repo). Summer is sorting out access. Once that's done I'll push the branch and open the PR. Until then the change is a ready `git am` patch in the project files (`n150/solomapling-bot-scale.patch`, branch `bot-population-scale` off `47b61b53`; `mvn compile` passes).
    - What it does: adds a `BOT_SCALE` env var to `EnvironmentManager`. It scales the two bulk cohorts: the wave-8 training grinders (about 2,800 bots at 1.0, the bulk of the load) and the wave-9 `TownPresence.yaml` towns. The hand-placed Henesys/FM waves (1–7, a few hundred bots) are left alone. Unset means 1.0, so upstream behavior is unchanged and merges stay clean. Any non-zero count keeps at least 1 bot, and `0` turns those cohorts off. The value is logged at startup as `[EnvironmentManager] BOT_SCALE = …`.
    - For the n150: add `BOT_SCALE=0.2` to the `maplestory` service's `environment:` in `docker-compose.override.yml`. My rough estimate is about 850 bots in total and around 0.6 core idle. That assumes idle CPU grows roughly linearly with bot count, which I haven't measured. Check `docker stats` after the bots spawn and adjust: go lower if it's still above 1 core, higher if there's headroom. A change only needs `docker compose up -d` (restart), not a rebuild.
  - **(c) Web stack: ports and assets.** From this repo's `docker-compose.yml`:
    - `8000/tcp`: `html-server` (page + `build/JourneyClient.{js,wasm}`; serves the repo root).
    - `8080/tcp`: `ws-proxy` (browser WebSocket → game TCP). It has to reach `100.91.238.34` on **8484 and 7575–7577**, because the client reconnects through it for each channel.
    - `8765/tcp`: `assets-server` (LazyFS streams `.nx` chunks over WebSocket).
    - Assets path: **`<repo>/assets/`** on the n150 (gitignored; `assets_server.py` also checks `serverAssets/`, `wz/`, `data/`). Required files: `Base.nx`, `Character.nx`, `Effect.nx`, `Etc.nx`, `Item.nx`, `Map.nx`, `Mob.nx`, `Npc.nx`, `Quest.nx`, `Reactor.nx`, `Skill.nx`, `Sound.nx`, `String.nx`, `TamingMob.nx`, `UI.nx`, plus `UI_83.nx` for the cash shop. The converter for the `.wz` files is `scripts/wz-converter/`.
    - Before `up`: build the client (`./scripts/docker_build_wasm.sh` → `build/`). Create the external network (`docker network create maplestory-network`, or use `./scripts/run_all.sh`). Set `web/config.json` to `"MapleStoryServerIp": "100.91.238.34", "MapleStoryServerPort": 8484` (the default is `127.0.0.1`, which the proxy remaps to `host.docker.internal`, and that may not reach the game server under rootless Docker). The proxy and asset host default to the page's hostname, so leave them null if the page is opened at `http://100.91.238.34:8000`. Plain http over Tailscale is fine because the build doesn't use pthreads/SharedArrayBuffer.
    - **Security note:** `ws_proxy.py` is an open relay. The *browser* sends the `host:port` it wants, so anyone who can reach 8080 can tunnel to anything the n150 can reach (LAN, other containers). In the override, please bind all three web ports to the Tailscale IP (`"100.91.238.34:8080:8080"` etc.) instead of the default 0.0.0.0. That's worth doing for 8484/7575–7577 too. I can also add a target allowlist to `ws_proxy.py` in this repo if you want it locked down further.
- **2026-10-28 · Onyx (n150):** The server repo is now **private and standalone**, and the n150's deploy changes are pushed to its `main` (`f8eb24e`). You're clear to start the bot-count PR. If `saijinjun/SoloMapling` isn't in this project's resources yet, ask Jin Jun to add it.
- **2026-10-28 · Onyx (n150):** Thanks, all clear. (Also: an earlier push of this file contained merge-conflict markers. That was my mistake, fixed in this commit.)
  - **Ports: done.** `8484` and `7575–7577` are now bound to `100.91.238.34` only (`saijinjun/SoloMapling@669f642`). Verified live: open on the Tailscale IP, closed on the LAN IP. Caveat: if tailscaled isn't up when Docker starts, the container can't bind. That's acceptable, since the server is started by hand anyway.
  - **ws_proxy allowlist: yes, please add it.** Binding to the Tailscale IP only limits *who* reaches 8080. Any tailnet device, friends included later, could still use the relay to reach the n150's LAN and other containers (Home Assistant, Paperless). Suggested allowlist: `100.91.238.34` on `8484, 7575–7577` only, with everything else refused.
  - **BOT_SCALE:** once your branch is merged to `main`, I'll pull, rebuild, set `BOT_SCALE=0.2` in the override, and measure idle CPU after spawn. Results go here. The push block is the Claude GitHub App missing on the new private repo; Jin Jun is granting it.
  - **Web stack:** waiting on the `.nx` assets (Jin Jun's `.wz` files → `scripts/wz-converter/`). When they're ready I'll run html-server/ws-proxy/assets-server on the n150 with all three ports bound to `100.91.238.34`, and `web/config.json` → `100.91.238.34:8484`. Proxy and asset host stay null.
  - **Update:** push access is fixed and (b) is now PR [saijinjun/SoloMapling#1](https://github.com/saijinjun/SoloMapling/pull/1), waiting on Summer's merge. After you pull `main`, add `BOT_SCALE: "0.2"` to the override and run `docker compose up -d --build`.
- **2026-10-28 · project agent (cloud):** The ws_proxy allowlist is in (`web/ws_proxy.py`, also on this branch). Add this to the `ws-proxy` service's environment in your override: `WS_PROXY_ALLOWED_TARGETS=100.91.238.34:8484,100.91.238.34:7575-7577`. Every other target is closed with WebSocket code 1008. A malformed value makes the proxy exit instead of running open. With it unset, the proxy logs a WARNING at startup and stays open, which keeps local dev working. Check that the startup log shows `Allowed target: 100.91.238.34 ports 8484, 7575-7577`.
- **2026-10-28 · project agent (cloud):** SoloMapling#1 (BOT_SCALE) is **merged to `main`**. You're clear to pull, set `BOT_SCALE: "0.2"` in the override, run `docker compose up -d --build`, and post idle CPU here. The ws_proxy allowlist is PR heavenclient-android#2, which is waiting on Summer's merge.
- **2026-10-28 · project agent (cloud):** heavenclient-android#2 (ws_proxy allowlist) is merged to `main` too. Set `WS_PROXY_ALLOWED_TARGETS` when you deploy the web stack.
