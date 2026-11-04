# Android build

The phone client is **OpenStory** (`src/openstory`), installed as
`com.saijinjun.heavenclient.openstory`. It is a native C++ client: SDL2
provides the window, touch and keyboard input, and audio, and rendering uses
OpenGL ES 3. The older Journey client (`src/client`, the same code as the
browser build) is still in the project as the `journey` build flavor, but it
is not what we test on phones. See [Journey (older client)](#journey-older-client).

## Getting an APK

The n150 checks every pull request to `main` (and stacked PRs) and posts the
result as the `n150/ci` commit status: native tests, the wasm build, and a
desktop build of OpenStory and Journey. A green `n150/ci` is the gate for
merging. When a commit goes green, the n150 builds the OpenStory APK (arm64,
pointed at the n150 server) and sends it to the project's Telegram, so nobody
needs to build or upload APKs by hand.

Server setup and deploy notes for the n150 are kept in the private server
repo, not here: this repo is public and holds only client code.

Each PR description has a **Phone test** checklist: what to tap and what you
should see. Test from it and report back on the PR's thread.

To build locally you need the Android SDK with NDK 27.2.12479018:

```bash
./android/fetch-deps.sh          # downloads SDL2 and FreeType sources
cd android && ./gradlew assembleOpenstoryRelease
```

The APK is written to `android/app/build/outputs/apk/openstory/release/`. It
is signed with the debug key, which is fine for sideloading but not for a
store.

## First start

1. Install the APK, turn on Tailscale on the phone, and make sure the game
   server and the web stack are running on the n150.
2. Open the app. The setup screen checks it can reach the n150 and offers:
   - **Play now:** the game starts right away and loads its `.nx` files from
     the server in 512 KB pieces as it reads them. Each piece is kept on the
     phone, so a new area takes a moment only the first time. Later starts
     go straight to the game.
   - **Download all:** fetches all 17 files (several GB) first, then plays
     without waiting. Wi-Fi is recommended. If the download stops, open the
     app again and tap **Resume**.

The files come from the web stack's `assets/` folder on the server, e.g.
`http://<n150-tailscale-ip>:8090/assets/`, and the game connects to
`<n150-tailscale-ip>:8484`. Both are only reachable over Tailscale. The
addresses are not in this public repo; they are set at build time:

- Gradle properties: `./gradlew assembleRelease -PserverIp=<address> -PassetsUrl=<url>`,
  or put `serverIp=` and `assetsUrl=` in `~/.gradle/gradle.properties`.
- Or environment variables `HEAVEN_SERVER_IP` and `HEAVEN_ASSETS_URL`.
- The GitHub `android-build.yml` workflow reads the repository variables
  `HEAVEN_SERVER_IP` and `HEAVEN_ASSETS_URL`; the n150 sets its own.

With neither set, the APK connects to `127.0.0.1` and streaming is off. The
game's own `AssetsUrl` setting can point it elsewhere later.

The files land in `Android/data/com.saijinjun.heavenclient.openstory/files/`. A
streamed file is kept as `<name>.cache` plus a small `<name>.lazy` map of the
pieces it has; once every piece is there it becomes a normal `<name>`.
Copying whole files there over USB also works. To start over, clear the
app's storage in Android settings.

Streaming is on by default for Android builds (`OPENSTORY_STREAM_ASSETS`,
which turns on lazy NX loading with `NX_LAZY_FILES`).

## Controls

On the game screen the app draws touch controls. On a wide phone they sit in
the black bars beside the game:

- **Left:** an analog stick. Put your thumb anywhere in the lower left and
  the stick appears under it; push to walk, climb ropes (up/down) and enter
  portals (up). Diagonals press both directions, e.g. down and left.
- **Top left:** inventory, equipment, stats, skills, quests and world map.
- **Bottom right:** attack (large, orange), jump (blue) and pick up (green).
  Two small buttons at the right edge combine keys: the lower one jumps down
  through a platform (down + jump), the upper one is a jump attack.
- **Right, around attack:** six quick slots. They are the Shift, Insert,
  Home, Page Up, Delete and End keys, and show the icon of the skill or item
  on that key. To set one, open the skills or inventory panel, tap a skill
  or a Use, Set-up or Cash item, and tap **Quick slot 1–6** (dragging onto
  a slot also works). It is saved to your key setup on the server.
  Empty slots show a faint **+** and ignore touches. Potions show how many
  are left, and a skill's slot darkens while it cools down.

Several fingers work at once, so you can walk, jump, attack and use skills
together. Anywhere else, a tap is a left click and a quick double tap is a
double click (talk to an NPC, use an item). Tapping a text field opens the
phone keyboard. The back button acts as Escape.

Buttons give a short vibration and a ripple when pressed
(`TouchVibration` turns the vibration off).

Ropes and ladders can be grabbed from about a character's width away
(`RopeAssist`), since a thumb on a stick is less exact than arrow keys.

The gear at the top right opens the layout editor. Drag any control to move
it (the stick sets where it rests). The toolbar at the top makes the picked
control smaller or bigger, steps all controls through solid, 75%, 50% and 30%
opacity, resets the default layout, and the green check closes the editor.
The layout is saved in the `TouchLayout` setting.

## Phone panels

On a phone, the inventory, stats, skills, quests, NPC shop and Cash Shop
open as full-screen touch panels instead of the classic windows: big rows,
tabs, a detail pane, and buttons in place of drag and drop. The skills panel
also has a **Classic view** button for the old skill book. The **Trade** button on the status bar opens the Maple Guide
(the server's `@guide` menu) instead of the MTS, since typing commands on a
phone is awkward.

The control artwork is drawn at startup from simple shapes
(`src/openstory/platform/sdl/TouchSprites.cpp`), so it stays sharp on any
screen.

## Journey (older client)

Build it with `./gradlew assembleJourneyRelease`; it installs as
`com.saijinjun.heavenclient` beside OpenStory. These options exist only in
Journey:

While playing on a wide phone the game fills the screen (`FillScreen`): the
view widens to the screen's shape and is sized so each game pixel is a whole
number of screen pixels (1400x630 at exactly 2x on a 2800x1260 phone). The
status bar is centered and windows open around the middle. Text is
rasterized at the screen's resolution, so it stays sharp.

The game world gets an "HD-2D" look (`src/client/Graphics/PostProcess.cpp`),
each part with its own setting, all on by default:

- `HdUpscale`: an xBR-style pixel-art upscaler that smooths diagonal edges.
- `HdGlow`: soft glow around bright parts.
- `HdDepthBlur`: the far background is blurred and slightly hazy.
- `HdColorGrade`: a gentle color grade and vignette.

The UI is drawn after these effects, so it stays crisp.

To save battery and heat, `EffectsQuality` turns effects down as a group: 2
(default) uses all of the above that are on, 1 only the cheap upscale and
color grade, 0 none. `MaxFps` (default 60, 0 for no limit) caps how many
frames are drawn per second, which matters on 90 and 120 Hz screens; the game
runs at the same speed either way.

## Desktop SDL build (Journey)

```bash
sudo apt-get install libsdl2-dev libfreetype-dev libgles-dev zlib1g-dev
cmake -S src/client -B build/sdl -G Ninja && cmake --build build/sdl
./build/sdl/JourneyClient /path/to/folder/with/nx/files
```
