# n150 server — status & handoff

> Shared channel between the claude.ai project agent (client) and Onyx on the n150 (server host).
> Read this at the start of each session; append replies/questions under **Thread** at the bottom.

## Where the server runs
- Host: **nexus-server (Intel N150, 4 cores / 16 GB)**, Ubuntu, rootless Docker. Always on.
- Reachable **only over Tailscale**: `100.91.238.34`. Nothing is exposed to the internet.
- Login `8484`, channels `7575–7577` (world 0, ch 1–3). Verified online 2026-10-28 ("Cosmic is now online").
- Code: `~/solomapling` = upstream `MadaraGameDev/SoloMapling` @ `47b61b53`. Will track the fork **`saijinjun/SoloMapling`** (`main` = what the n150 deploys) once the first push lands.

## Server changes already made on the n150 (pending push to the fork)
- `config.yaml`: `HOST`, `LANHOST`, `LOCALHOST` all = `100.91.238.34`. Rootless Docker can hide the client's real source IP, so all three point at the one address every client can reach. **Every client must be on Tailscale.**
- `docker-compose.override.yml`, kept separate so upstream merges stay clean:
  - `restart: unless-stopped`; `JAVA_TOOL_OPTIONS=-Xmx4g`; `cpus: 3.0` (leaves a core for the box's other services).
  - **MySQL port no longer published** (it has an empty root password). Admin: `docker compose exec db mysql`.
  - **Bug fix:** SoloMapling reads `src/main/java/soloMapling/**` (bot names, item pools, dialogue YAML) at runtime, but the upstream Dockerfile ships only the jar, so every bot spawn threw `FileNotFoundException`. Fixed with a read-only mount of that folder. 0 errors after the fix.

## Current state: STOPPED (intentionally)
- Idle cost with **zero players**: about **2.2 cores** steady, the full 3-core cap during bot spawn, 2.5 GB RAM. Too heavy to leave running 24/7 on a shared box before any client can connect.
- Start/stop on the n150: `docker compose start` / `stop` in `~/solomapling`.

## Asks for the project agent
1. **Reduce bot population** so idle CPU stays under 1 core. Knobs: `src/main/java/soloMapling/ArtificialPlayer/BotTownSystem/TownPresence.yaml` and `src/main/java/soloMapling/Environment/EnvironmentManager.java`. Do this as a PR to `saijinjun/SoloMapling`.
   - Note: this repo's `AGENTS.md` says the server is off-limits. That's upstream maplestory-wasm wording. For this project, server changes are fine, but they go through the fork, never this repo.
2. **Web client wiring:** this repo already ships `web/ws_proxy.py` + the `ws-proxy` compose service. Plan is to run that on the n150 too, pointing at `100.91.238.34:8484`. Tell us which ports and assets path the web stack needs (`.nx` assets once Jin Jun supplies the `.wz` files) and Onyx will deploy it.
3. Keep all secrets out of both repos. DB credentials live only on the n150.

## How changes flow
- Client code lives here. Server code and config live in `saijinjun/SoloMapling`, changed via branch → PR → Jin Jun merges → Onyx pulls `main` on the n150 and runs `docker compose up -d --build`.
- The n150 has write deploy keys for both repos. It never accepts inbound connections from the cloud.

## Thread
- **2026-10-28 · Onyx (n150):** Server built, verified online, bot-data bug fixed, then stopped to save CPU. Waiting on: (a) the fork's first push of the changes above, (b) the bot-count PR, (c) your web-stack port/asset list.
