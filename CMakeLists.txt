cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")

project(Sleepcousonique VERSION 1.4.1 LANGUAGES C CXX)
# the SP-1 firmware that ships with this editor (firmware/dist/sleepcousonique-sp1.bin, SCQ_VERSION
# in firmware/app/src/sp1_scq.c): the editor tells an SP-1 running an older one to update
set(SCQ_FIRMWARE_VERSION "1.4.0")

# Windows: the C runtime linked into the .exe, so it starts on a PC without the Visual C++
# Redistributable (no VCRUNTIME140.dll / MSVCP140.dll to install)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

# Legacy builds (-DSCQ_LEGACY=ON, with -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13): both apps as
# "... Legacy", drawn flat and redrawn only where something moved, for old computers.
option(SCQ_LEGACY "Build the light legacy versions of the apps" OFF)
if(SCQ_LEGACY)
    set(SCQ_SUFFIX " Legacy")
    set(SCQ_ID_SUFFIX ".legacy")
else()
    set(SCQ_SUFFIX "")
    set(SCQ_ID_SUFFIX "")
endif()

# JUCE: a local checkout in ./JUCE, or FETCHCONTENT_SOURCE_DIR_JUCE, or the pinned release.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.5
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# The engine: freestanding C, shared with the SP-1 firmware.
add_library(scq_engine STATIC engine/scq_core.c engine/scq_places.c)
target_include_directories(scq_engine PUBLIC engine)
target_compile_definitions(scq_engine PUBLIC SCQ_HOST)

add_executable(scqtool tools/scqtool.c)
target_link_libraries(scqtool PRIVATE scq_engine)
if(UNIX)
    target_link_libraries(scqtool PRIVATE m)
endif()

# every place, every inlet, a fader patched in and its attenuverter at both ends: nothing may
# go silent, stick or stop being a number, and everything comes back with no restart
add_executable(ScqSweep tests/sweep.c)
target_link_libraries(ScqSweep PRIVATE scq_engine)
if(UNIX)
    target_link_libraries(ScqSweep PRIVATE m)
endif()

set_property(SOURCE app/Serial.cpp APPEND PROPERTY COMPILE_DEFINITIONS "SCQ_FIRMWARE_VERSION=\"${SCQ_FIRMWARE_VERSION}\"")

juce_add_gui_app(Sleepcousonique
    PRODUCT_NAME "Sleepcousonique${SCQ_SUFFIX}"
    VERSION 1.4.1
    COMPANY_NAME "Sleepcousonique"
    BUNDLE_ID "com.sleepcousonique.editor${SCQ_ID_SUFFIX}"
    DOCUMENT_EXTENSIONS dream
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/app/icon/icon.png"
    MICROPHONE_PERMISSION_ENABLED FALSE
    BLUETOOTH_PERMISSION_ENABLED TRUE
    BLUETOOTH_PERMISSION_TEXT "A dream with a crowd in it listens for the Bluetooth devices nearby: how many there are and how strong. Nothing about them is kept or sent.")

file(GLOB SCQ_SCALE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/scales/*.scl)
juce_add_binary_data(ScqData HEADER_NAME ScqData.h NAMESPACE ScqData
    SOURCES examples/first-night.dream examples/a-history.dream docs/places.md ${SCQ_SCALE_FILES}
            app/fonts/Inter-Regular.ttf app/fonts/Inter-Medium.ttf app/fonts/IBMPlexMono-Regular.ttf)

target_sources(Sleepcousonique PRIVATE
    app/Main.cpp
    app/Model.cpp
    app/Host.cpp
    app/Art.cpp
    app/Tapestry.cpp
    app/Journal.cpp
    app/Hands.cpp
    app/Serial.cpp
    app/Scales.cpp
    app/Router.cpp
    app/Pop.cpp
    app/Timeline.cpp
    app/Radio.cpp
    app/MainComponent.cpp)
if(APPLE)
    # the room: CoreBluetooth hears the devices nearby (the crowd and visitors places)
    target_sources(Sleepcousonique PRIVATE app/RadioMac.mm)
    set_source_files_properties(app/RadioMac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(Sleepcousonique PRIVATE "-framework CoreBluetooth")
endif()

target_compile_definitions(Sleepcousonique PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0
    SCQ_LEGACY=$<BOOL:${SCQ_LEGACY}>
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Sleepcousonique,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Sleepcousonique,JUCE_VERSION>")

target_link_libraries(Sleepcousonique
    PRIVATE
        scq_engine
        ScqData
        juce::juce_gui_extra
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# tests for the text edits the tapestry makes
juce_add_console_app(ScqTests PRODUCT_NAME "ScqTests")
target_sources(ScqTests PRIVATE tests/ModelTest.cpp app/Model.cpp app/Scales.cpp app/Radio.cpp)
if(APPLE)
    target_sources(ScqTests PRIVATE app/RadioMac.mm)
    target_link_libraries(ScqTests PRIVATE "-framework CoreBluetooth")
endif()
target_compile_definitions(ScqTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(ScqTests PRIVATE scq_engine ScqData juce::juce_core juce::juce_graphics PUBLIC juce::juce_recommended_config_flags)

# the USB link against a pretend SP-1 (tests/fakesp1.c: the firmware's protocol, the real engine)
juce_add_console_app(ScqSerialTest PRODUCT_NAME "ScqSerialTest")
target_sources(ScqSerialTest PRIVATE tests/SerialTest.cpp app/Serial.cpp tests/fakesp1.c)
target_compile_definitions(ScqSerialTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(ScqSerialTest PRIVATE scq_engine juce::juce_core PUBLIC juce::juce_recommended_config_flags)

# the same pretend SP-1 behind a pseudo-terminal, for the real app (tools/test_send.sh; not Windows)
if(UNIX)
    add_executable(fakesp1 tests/fakesp1_pty.c tests/fakesp1.c)
    target_link_libraries(fakesp1 PRIVATE scq_engine m)
endif()

# Sleepcousonique Array: SP-1s in interface mode as one multichannel output (macOS)
juce_add_gui_app(SleepcousoniqueArray
    PRODUCT_NAME "Sleepcousonique Array${SCQ_SUFFIX}"
    VERSION 1.4.1
    COMPANY_NAME "Sleepcousonique"
    BUNDLE_ID "com.sleepcousonique.array${SCQ_ID_SUFFIX}"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/array/icon/icon.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "The Array reads a loopback input (BlackHole) to send it to your SP-1s.")
juce_add_binary_data(ArrayFonts HEADER_NAME ArrayFonts.h NAMESPACE ArrayFonts
    SOURCES array/fonts/Inter-Regular.ttf array/fonts/Inter-Medium.ttf array/fonts/IBMPlexMono-Regular.ttf array/icon/logo.png)
target_sources(SleepcousoniqueArray PRIVATE array/Main.cpp array/Aggregate.cpp app/Serial.cpp)
target_compile_definitions(SleepcousoniqueArray PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0 SCQ_LEGACY=$<BOOL:${SCQ_LEGACY}>)
target_link_libraries(SleepcousoniqueArray
    PRIVATE ArrayFonts juce::juce_gui_extra juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)
if(APPLE)
    target_link_libraries(SleepcousoniqueArray PRIVATE "-framework CoreAudio" "-framework CoreFoundation")
endif()
