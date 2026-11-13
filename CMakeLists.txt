cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")

project(Sleepcousonique VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

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

juce_add_gui_app(Sleepcousonique
    PRODUCT_NAME "Sleepcousonique"
    VERSION 0.1.0
    COMPANY_NAME "Sleepcousonique"
    BUNDLE_ID "com.sleepcousonique.editor"
    DOCUMENT_EXTENSIONS dream
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/app/icon/icon.png"
    MICROPHONE_PERMISSION_ENABLED FALSE)

juce_add_binary_data(ScqData HEADER_NAME ScqData.h NAMESPACE ScqData
    SOURCES examples/first-night.dream examples/a-history.dream docs/places.md)

target_sources(Sleepcousonique PRIVATE
    app/Main.cpp
    app/Model.cpp
    app/Host.cpp
    app/Art.cpp
    app/Tapestry.cpp
    app/Journal.cpp
    app/Hands.cpp
    app/Serial.cpp
    app/MainComponent.cpp)

target_compile_definitions(Sleepcousonique PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0
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
target_sources(ScqTests PRIVATE tests/ModelTest.cpp app/Model.cpp)
target_compile_definitions(ScqTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(ScqTests PRIVATE scq_engine juce::juce_core juce::juce_graphics PUBLIC juce::juce_recommended_config_flags)
