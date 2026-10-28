## OpenStory (rdiol12/OpenStory) built for SDL2 + OpenGL ES 3: the Android
## port, and a Linux desktop build of the same code for testing.
## Upstream's own CMakeLists (Windows/macOS, GLFW + BASS) is not used here.
cmake_minimum_required(VERSION 3.14)
project(OpenStory CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(NLNX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/nlnx")

file(GLOB_RECURSE SOURCES "src/*.cpp")

# Replaced by the SDL platform layer in platform/sdl.
list(FILTER SOURCES EXCLUDE REGEX ".*/src/IO/Window\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX ".*/src/IO/Gamepad\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX ".*/src/Audio/Audio\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX ".*/src/Net/SocketWinsock\\.cpp$")

list(APPEND SOURCES
    platform/sdl/WindowSDL.cpp
    platform/sdl/GamepadSDL.cpp
    platform/sdl/AudioSDL.cpp
    platform/sdl/ClipAudioSDL.cpp
    platform/sdl/StartSDL.cpp
    platform/sdl/TouchControls.cpp
    platform/sdl/TouchSprites.cpp
    ${NLNX_DIR}/audio.cpp
    ${NLNX_DIR}/bitmap.cpp
    ${NLNX_DIR}/file.cpp
    ${NLNX_DIR}/node.cpp
    ${NLNX_DIR}/nx.cpp
)
set(C_SOURCES vendor/lz4.c)

# Streaming: .nx files missing on the device are fetched in pieces over HTTP
# as the game reads them, instead of being downloaded whole first. On by
# default for Android.
if(ANDROID)
    set(STREAM_DEFAULT ON)
else()
    set(STREAM_DEFAULT OFF)
endif()
option(OPENSTORY_STREAM_ASSETS "Fetch missing .nx files on demand over HTTP" ${STREAM_DEFAULT})
set(OPENSTORY_ASSETS_URL "" CACHE STRING "Default AssetsUrl: where streamed .nx files come from")
set(OPENSTORY_DEFAULT_SERVER_IP "" CACHE STRING "Default ServerIP for new Settings files")

if(OPENSTORY_STREAM_ASSETS)
    list(APPEND SOURCES
        platform/sdl/LazyFS/LazyFileLoader.cpp
        platform/sdl/LazyFS/LazyFileBackendNative.cpp
        platform/sdl/LazyFS/HttpRange.cpp)
endif()

if(ANDROID)
    # SDL's Java activity loads the game as libmain.so.
    add_library(OpenStory SHARED ${SOURCES} ${C_SOURCES})
    set_target_properties(OpenStory PROPERTIES OUTPUT_NAME main)
else()
    add_executable(OpenStory ${SOURCES} ${C_SOURCES})
endif()

target_compile_definitions(OpenStory PRIVATE
    MS_PLATFORM_SDL=1
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED)
if(OPENSTORY_STREAM_ASSETS)
    target_compile_definitions(OpenStory PRIVATE NX_LAZY_FILES=1)
endif()
if(OPENSTORY_ASSETS_URL)
    target_compile_definitions(OpenStory PRIVATE OPENSTORY_ASSETS_URL="${OPENSTORY_ASSETS_URL}")
endif()
if(OPENSTORY_DEFAULT_SERVER_IP)
    target_compile_definitions(OpenStory PRIVATE OPENSTORY_DEFAULT_SERVER_IP="${OPENSTORY_DEFAULT_SERVER_IP}")
endif()

target_include_directories(OpenStory PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/IO          # "KeyCodes.h" lookups from src/IO
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/shared # PlatformConfig.h, GLCompat.h, KeyCodes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/sdl    # <LazyFS/...>
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor          # <lz4.h>, <minimp3/...>
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb      # <stb_image.h>
    ${NLNX_DIR}/..                              # <nlnx/nx.hpp>
)

target_compile_options(OpenStory PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-include ${CMAKE_CURRENT_SOURCE_DIR}/platform/sdl/Prelude.h>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-c++11-narrowing -Wno-narrowing -Wno-deprecated-declarations>
    -fexceptions)

if(ANDROID)
    # The Gradle build passes SDL2_SOURCE_DIR, FREETYPE_SOURCE_DIR and ASIO_INCLUDE_DIR.
    if(NOT TARGET SDL2::SDL2)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        add_subdirectory("${SDL2_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/SDL2")
    endif()
    if(NOT TARGET freetype)
        set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
        add_subdirectory("${FREETYPE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/freetype")
    endif()
    target_include_directories(OpenStory PRIVATE "${ASIO_INCLUDE_DIR}")
    target_link_libraries(OpenStory PRIVATE SDL2::SDL2 freetype GLESv3 z log android)
else()
    find_package(SDL2 REQUIRED)
    find_package(Freetype REQUIRED)
    find_package(ZLIB REQUIRED)
    find_path(ASIO_INCLUDE_DIR asio.hpp REQUIRED)
    find_library(GLESV2_LIBRARY NAMES GLESv2 REQUIRED)
    target_include_directories(OpenStory PRIVATE "${ASIO_INCLUDE_DIR}")
    target_link_libraries(OpenStory PRIVATE SDL2::SDL2 Freetype::Freetype ZLIB::ZLIB ${GLESV2_LIBRARY} pthread)
endif()
