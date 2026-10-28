# Android build

The Android app runs the same C++ client as the browser build. SDL2 provides
the window, touch and keyboard input, and audio. Rendering uses OpenGL ES 3,
the same renderer path as WebGL 2. A desktop SDL build exists for quick checks
on Linux.

## Getting an APK

Every push and pull request to `main` builds a release APK in the
`android-build` workflow. Download the `heavenclient-apk` artifact from the run
and install it on the phone (allow installing from unknown sources).

To build locally you need the Android SDK with NDK 27.2.12479018:

```bash
./android/fetch-deps.sh          # downloads SDL2 and FreeType sources
cd android && ./gradlew assembleRelease
```

The APK is written to `android/app/build/outputs/apk/release/`. It is signed
with the debug key, which is fine for sideloading but not for a store.

## Connecting to the server

The APK is built to connect to the n150 game server at `100.91.238.34:8484`.
That address is only reachable over Tailscale, so the phone must have the
Tailscale app running and signed in to the same tailnet. To build for another
server, pass `-PserverIp=<address>` to Gradle, or edit `MapleStoryServerIp` in
the `Settings` file after the first start.

## Game files

Start the app once. It creates its data folder and a `Settings` file in it:

```
Android/data/com.saijinjun.heavenclient/files/
```

Connect the phone to a computer over USB to reach that folder and copy these
17 `.nx` files into it (several GB, so leave room on the phone):

`Base`, `Character`, `Effect`, `Etc`, `Item`, `Map`, `Mob`, `Morph`, `Npc`,
`Quest`, `Reactor`, `Skill`, `Sound`, `String`, `TamingMob`, `UI` (v153+), and
`UI_83` (cash shop).

The same set is in `assets/` on the n150. If a file is missing, the app shows
a message naming it and the folder path.

## Controls

On the game screen the app draws touch controls. On a wide phone they sit in
the black bars beside the game:

- **Bottom left:** a d-pad. Slide your thumb around it to walk, climb ropes
  (up/down) and enter portals (up). Diagonals press both directions, e.g.
  down and left.
- **Bottom right:** attack (red, large), jump (green, left of attack) and
  pick up (yellow, above attack).

Several fingers work at once, so you can walk, jump and attack together.
Anywhere else, a tap is a left click and a quick double tap is a double click
(talk to an NPC, use an item). Tapping a text field opens the phone keyboard.
The back button acts as Escape. Skills and items are used from the quick
slots for now; dedicated skill buttons are not added yet.

## Desktop SDL build

```bash
sudo apt-get install libsdl2-dev libfreetype-dev libgles-dev zlib1g-dev
cmake -S src/client -B build/sdl -G Ninja && cmake --build build/sdl
./build/sdl/JourneyClient /path/to/folder/with/nx/files
```
