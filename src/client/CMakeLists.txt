## CMake file for the JourneyClient from-scratch Maplestory client ##
cmake_minimum_required (VERSION 3.8)
project (JourneyClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DEBUG_SYMBOLS "Enable debug symbols (-g)" OFF)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(ERROR_ON_UNUSED_VARIABLES "Fail the build on unused variable warnings" ON)
option(ENFORCE_UNUSED_INCLUDES "Run include-what-you-use to catch unused includes" ON)

if(EMSCRIPTEN)
    add_compile_definitions(MS_PLATFORM_WASM=1)
    add_compile_options("-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=/")
    add_compile_options(
        --use-port=freetype
        -Wno-c++11-narrowing
        -Wno-deprecated-declarations
        -fexceptions
    )
    # Emscripten -s flags are linker settings; passing them at compile time emits warnings.
    add_link_options(
        -sUSE_GLFW=3 
        -sUSE_WEBGL2=1 
        -sFULL_ES3=1
        -sWASM=1 
        -sALLOW_MEMORY_GROWTH=1
        -sFORCE_FILESYSTEM=1
        -fexceptions
        -sUSE_ZLIB=1
        --use-port=freetype
        -lwebsocket.js 
        -sASYNCIFY=1
        -sASYNCIFY_IMPORTS=['emscripten_asm_const_int','emscripten_asm_const_double','emscripten_sleep']
        -sASYNCIFY_STACK_SIZE=16777216
        -sSTACK_SIZE=16777216
        -sINITIAL_MEMORY=268435456  
        -sEXPORTED_RUNTIME_METHODS=FS,ccall,cwrap,setCanvasSize
        "--embed-file=${CMAKE_CURRENT_SOURCE_DIR}/fonts/Arial/Arial.ttf@fonts/Arial/Arial.ttf"
        "--embed-file=${CMAKE_CURRENT_SOURCE_DIR}/fonts/Arial/Arial-Bold.ttf@fonts/Arial/Arial-Bold.ttf"
        --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/LazyFS/lazyfs.js
        -sASSERTIONS=1
    )
    
    if(DEBUG_SYMBOLS)
        add_compile_options(-g)
        add_link_options(-g3 -gsource-map)
    endif()
else()
    # Desktop and Android builds: SDL2 for the window, input and audio, and
    # OpenGL ES 3 for rendering (the same renderer path as WebGL 2).
    add_compile_definitions(MS_PLATFORM_SDL=1)
    # Server address a fresh install starts with; Settings can still change it.
    set(JOURNEY_DEFAULT_SERVER_IP "" CACHE STRING "Default MapleStoryServerIp for new Settings files")
    if(JOURNEY_DEFAULT_SERVER_IP)
        add_compile_definitions(JOURNEY_DEFAULT_SERVER_IP="${JOURNEY_DEFAULT_SERVER_IP}")
    endif()
    add_compile_options(-Wno-c++11-narrowing -Wno-narrowing -Wno-deprecated-declarations -fexceptions)
endif()

file(GLOB_RECURSE SOURCES
    "Audio/*.cpp" "Audio/*.c"
    "Character/*.cpp" "Character/*.c"
    "Data/*.cpp" "Data/*.c"
    "Gameplay/*.cpp" "Gameplay/*.c"
    "Graphics/*.cpp" "Graphics/*.c"
    "IO/*.cpp" "IO/*.c"
    "LazyFS/*.cpp" "LazyFS/*.c"
    "Net/*.cpp" "Net/*.c"
    "Template/*.cpp" "Template/*.c"
    "Util/*.cpp" "Util/*.c"
)

list(APPEND SOURCES
    "Configuration.cpp"
    "Journey.cpp"
    "includes/lz4.c"
    "includes/stb/stb_vorbis.c"
    "../nlnx/bitmap.cpp"
    "../nlnx/node.cpp"
    "../nlnx/audio.cpp"
    "../nlnx/file.cpp"
    "../nlnx/nx.cpp"
)

if(NOT EMSCRIPTEN)
    # Browser-only code: the LazyFS asset streamer and the WebSocket transport.
    list(FILTER SOURCES EXCLUDE REGEX ".*/LazyFS/LazyFile(Backend|Loader)\\.cpp$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/Net/SockInternal\\.cpp$")
endif()

if(ANDROID)
    # SDL's Java activity loads the game as libmain.so.
    add_library(JourneyClient SHARED ${SOURCES})
    set_target_properties(JourneyClient PROPERTIES OUTPUT_NAME main)
else()
    add_executable(JourneyClient ${SOURCES})
endif()

if(NOT EMSCRIPTEN)
    if(ANDROID)
        # The Gradle build passes SDL2_SOURCE_DIR and FREETYPE_SOURCE_DIR.
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
        target_link_libraries(JourneyClient PRIVATE SDL2::SDL2 freetype GLESv3 z log android)
    else()
        find_package(SDL2 REQUIRED)
        find_package(Freetype REQUIRED)
        find_package(ZLIB REQUIRED)
        find_library(GLESV2_LIBRARY NAMES GLESv2 REQUIRED)
        target_link_libraries(JourneyClient PRIVATE SDL2::SDL2 Freetype::Freetype ZLIB::ZLIB ${GLESV2_LIBRARY} pthread)
        # The client copies these into its data folder on first start.
        add_custom_command(TARGET JourneyClient POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/fonts" "$<TARGET_FILE_DIR:JourneyClient>/fonts")
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    if(WARNINGS_AS_ERRORS)
        target_compile_options(JourneyClient PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-Werror>
            $<$<COMPILE_LANGUAGE:CXX>:-Werror>
        )
    endif()

    if(ERROR_ON_UNUSED_VARIABLES)
        target_compile_options(JourneyClient PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-Werror=unused-variable>
            $<$<COMPILE_LANGUAGE:CXX>:-Werror=unused-variable>
            $<$<COMPILE_LANGUAGE:C>:-Werror=unused-but-set-variable>
            $<$<COMPILE_LANGUAGE:CXX>:-Werror=unused-but-set-variable>
            $<$<COMPILE_LANGUAGE:CXX>:-Werror=unused-const-variable>
        )
    endif()
endif()

if(ENFORCE_UNUSED_INCLUDES)
    find_program(IWYU_EXECUTABLE NAMES include-what-you-use iwyu)
    if(IWYU_EXECUTABLE)
        set_target_properties(JourneyClient PROPERTIES
            CXX_INCLUDE_WHAT_YOU_USE "${IWYU_EXECUTABLE};-Xiwyu;--error=1"
        )
    else()
        message(STATUS "ENFORCE_UNUSED_INCLUDES is ON, but include-what-you-use is not installed. Unused include checks are skipped.")
    endif()
endif()

# Include directories
include_directories(".")
include_directories("..")
include_directories("../nlnx")
include_directories("includes")
include_directories("includes/stb")
include_directories("includes/GL")
