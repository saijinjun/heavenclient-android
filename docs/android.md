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

## First start

1. Install the APK, turn on Tailscale on the phone, and make sure the game
   server and the web stack are running on the n150.
2. Open the app. The setup screen shows how much it needs to download (the
   17 `.nx` files, several GB) and checks there is room on the phone.
3. Tap **Download**. Wi-Fi is recommended. The game starts by itself when the
   download finishes, and later starts open the game directly.

If the download stops (Wi-Fi drops, the app is closed), open the app again
and tap **Resume**: finished files are kept, and a half-done file continues
where it stopped. If the n150 can't be reached, the screen says so and offers
**Try again**.

The files come from the web stack's `assets/` folder at
`http://100.91.238.34:8090/assets/`, and the game connects to
`100.91.238.34:8484`. Both addresses are only reachable over Tailscale. To
build for another server, pass `-PassetsUrl=<url>` and `-PserverIp=<address>`
to Gradle.

The files land in `Android/data/com.saijinjun.heavenclient/files/`. Copying
them there over USB also works: the app skips any file that is already
present. To force a fresh download, clear the app's storage in Android
settings.

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
