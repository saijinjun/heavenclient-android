## Picks which game client the APK is built from. Gradle passes -DCLIENT per
## product flavor: "journey" (src/client) or "openstory" (src/openstory).
cmake_minimum_required(VERSION 3.14)
project(HeavenClientAndroid CXX C)

set(CLIENT "journey" CACHE STRING "Client to build: journey or openstory")

if(CLIENT STREQUAL "openstory")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../src/openstory" openstory)
elseif(CLIENT STREQUAL "journey")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../src/client" client)
else()
    message(FATAL_ERROR "Unknown CLIENT '${CLIENT}'")
endif()
