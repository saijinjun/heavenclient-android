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

## Game files

Start the app once. It creates its data folder and a `Settings` file in it:

```
Android/data/com.saijinjun.heavenclient/files/
```

Connect the phone to a computer over USB to reach that folder, then:

1. Copy every `.nx` file into it (see the asset list in the main README).
2. Open `Settings` and set `MapleStoryServerIp` to the server's address, for
   example the server PC's LAN or Tailscale IP. The port is 8484.

If something is missing, the app shows a message with the folder path.

## Controls

A tap is a left click and a double tap is a double click. Tapping a text
field opens the phone keyboard. The back button acts as Escape. Movement and
skill buttons for touch are not added yet.

## Desktop SDL build

```bash
sudo apt-get install libsdl2-dev libfreetype-dev libgles-dev zlib1g-dev
cmake -S src/client -B build/sdl -G Ninja && cmake --build build/sdl
./build/sdl/JourneyClient /path/to/folder/with/nx/files
```
