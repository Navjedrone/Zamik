cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary on macOS")

project(Zamik VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE: use ./JUCE if present, otherwise download JUCE 8.0.10
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.10
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

option(ZAMIK_BUILD_TESTS "Build the bit-transparency test runner" ON)

juce_add_plugin(Zamik
    COMPANY_NAME                "Zamik Audio"
    BUNDLE_ID                   "com.zamikaudio.zamik"
    PLUGIN_MANUFACTURER_CODE    Zmka
    PLUGIN_CODE                 Zmk1
    FORMATS                     AU VST3 Standalone
    PRODUCT_NAME                "Zamik"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Tools
    AU_MAIN_TYPE                kAudioUnitType_Effect)

juce_add_binary_data(ZamikAssets SOURCES
    Resources/fonts/BarlowCondensed-Medium.ttf
    Resources/fonts/BarlowCondensed-SemiBold.ttf
    Resources/fonts/JetBrainsMono-Regular.ttf
    Resources/fonts/JetBrainsMono-Medium.ttf
    Resources/logo.svg)

set(ZAMIK_SOURCES
    Source/Core.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/NixieDisplay.cpp
    Source/ZamikLookAndFeel.cpp)

target_sources(Zamik PRIVATE ${ZAMIK_SOURCES})

target_compile_definitions(Zamik PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Zamik
    PRIVATE
        ZamikAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Test runner: drives the real processor offline and checks every output
# word against the input, shifted by the expected number of samples.
# ---------------------------------------------------------------------------
if(ZAMIK_BUILD_TESTS)
    juce_add_console_app(ZamikTests PRODUCT_NAME "ZamikTests")
    target_sources(ZamikTests PRIVATE tests/Tests.cpp ${ZAMIK_SOURCES})
    target_compile_definitions(ZamikTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 ZAMIK_TESTS=1
        JucePlugin_Name="Zamik")
    target_link_libraries(ZamikTests PRIVATE
        ZamikAssets
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
