cmake_minimum_required(VERSION 3.22)

project(NinjamVST3 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS deployment target")
endif()

include(FetchContent)

FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.3
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  libogg
  GIT_REPOSITORY https://github.com/xiph/ogg.git
  GIT_TAG v1.3.5
)

FetchContent_Declare(
  libvorbis
  GIT_REPOSITORY https://github.com/xiph/vorbis.git
  GIT_TAG v1.3.7
)

FetchContent_MakeAvailable(juce libogg)

set(OGG_INCLUDE_DIR "${libogg_SOURCE_DIR}/include" CACHE PATH "" FORCE)
set(OGG_LIBRARY "ogg" CACHE STRING "" FORCE)

FetchContent_MakeAvailable(libvorbis)

add_library(ninjam_core STATIC
  ninjam/ninjam/njclient.cpp
  ninjam/ninjam/netmsg.cpp
  ninjam/ninjam/mpb.cpp
  ninjam/WDL/jnetlib/asyncdns.cpp
  ninjam/WDL/jnetlib/connection.cpp
  ninjam/WDL/jnetlib/util.cpp
  ninjam/WDL/rng.cpp
  ninjam/WDL/sha.cpp
  $<$<PLATFORM_ID:Windows>:ninjam/WDL/win32_utf8.c>
)

add_executable(ninjamsrv_local
  ninjam/ninjam/server/ninjamsrv.cpp
  ninjam/ninjam/server/usercon.cpp
  ninjam/ninjam/netmsg.cpp
  ninjam/ninjam/mpb.cpp
  ninjam/WDL/jnetlib/asyncdns.cpp
  ninjam/WDL/jnetlib/connection.cpp
  ninjam/WDL/jnetlib/httpget.cpp
  ninjam/WDL/jnetlib/listen.cpp
  ninjam/WDL/jnetlib/util.cpp
  ninjam/WDL/rng.cpp
  ninjam/WDL/sha.cpp
)

set_target_properties(ninjamsrv_local PROPERTIES OUTPUT_NAME "ninjamsrv")

target_include_directories(ninjamsrv_local
  PRIVATE
    ninjam/ninjam
    ninjam/WDL
)

target_compile_definitions(ninjamsrv_local
  PRIVATE
    $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    $<$<PLATFORM_ID:Windows>:_CRT_NONSTDC_NO_WARNINGS>
)

target_link_libraries(ninjamsrv_local
  PRIVATE
    $<$<PLATFORM_ID:Windows>:ws2_32>
)

target_include_directories(ninjam_core
  PUBLIC
    ninjam/ninjam
    ninjam/WDL
    ${libogg_SOURCE_DIR}/include
    ${libvorbis_SOURCE_DIR}/include
)

target_compile_definitions(ninjam_core
  PRIVATE
    $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    $<$<PLATFORM_ID:Windows>:_CRT_NONSTDC_NO_WARNINGS>
)

target_link_libraries(ninjam_core
  PRIVATE
    ogg
    vorbis
    vorbisenc
    $<$<PLATFORM_ID:Windows>:ws2_32>
)

juce_add_plugin(NinjamVST3
  COMPANY_NAME "Nykwil"
  BUNDLE_ID "com.nykwil.ninjamvst3"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE Nykw
  PLUGIN_CODE NjV3
  FORMATS VST3 AU
  PRODUCT_NAME "NINJAM VST3"
)

target_sources(NinjamVST3
  PRIVATE
    src/NinjamClientService.cpp
    src/NinjamClientService.h
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/PluginProcessor.cpp
    src/PluginProcessor.h
)

target_compile_definitions(NinjamVST3
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    $<$<PLATFORM_ID:Windows>:_CRT_NONSTDC_NO_WARNINGS>
)

target_link_libraries(NinjamVST3
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    ninjam_core
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

juce_generate_juce_header(NinjamVST3)
