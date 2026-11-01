cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architectures")

project(Eido VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: локально можно передать -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(EIDO_FORMATS VST3 AU)
else()
    set(EIDO_FORMATS VST3)
endif()

juce_add_plugin(Eido
    COMPANY_NAME "Personal"
    BUNDLE_ID "com.personal.eido"
    PLUGIN_MANUFACTURER_CODE Prsn
    PLUGIN_CODE Eido
    FORMATS ${EIDO_FORMATS}
    PRODUCT_NAME "Eido"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Analyzer
    AU_MAIN_TYPE kAudioUnitType_Effect)

set(EIDO_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Analysis/Loudness.cpp
    Source/UI/LookAndFeel.cpp
    Source/UI/Widgets.cpp
    Source/UI/Module.cpp
    Source/UI/WaveModules.cpp
    Source/UI/SpectrumModule.cpp
    Source/UI/MeterModules.cpp
    Source/UI/About.cpp)

target_sources(Eido PRIVATE ${EIDO_SOURCES})
target_include_directories(Eido PRIVATE Source)

set(EIDO_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_definitions(Eido PUBLIC ${EIDO_DEFS})

# Шрифты Medeo (SIL OFL): Sans — интерфейс, Mono — числа, Pixel — логотип. Встраиваются в плагин
juce_add_binary_data(EidoFonts
    NAMESPACE EidoFonts
    HEADER_NAME BinaryData.h
    SOURCES
        Resources/Fonts/MedeoSans-Regular.ttf
        Resources/Fonts/MedeoSans-SemiBold.ttf
        Resources/Fonts/MedeoMono-Regular.ttf
        Resources/Fonts/MedeoMono-SemiBold.ttf
        Resources/Fonts/MedeoPixel-Regular.ttf)
set_target_properties(EidoFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(EIDO_LIBS
    EidoFonts
    juce::juce_audio_utils
    juce::juce_dsp)

target_link_libraries(Eido
    PRIVATE ${EIDO_LIBS}
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Консольный тест: измерения (LUFS, корреляция, спектр), сквозной звук, скриншоты интерфейса
option(EIDO_BUILD_TESTS "Build offline test" OFF)
if(EIDO_BUILD_TESTS)
    juce_add_console_app(EidoTest PRODUCT_NAME "EidoTest")
    target_sources(EidoTest PRIVATE Tests/EidoTest.cpp ${EIDO_SOURCES})
    target_include_directories(EidoTest PRIVATE Source)
    target_compile_definitions(EidoTest PRIVATE ${EIDO_DEFS} EIDO_TEST_BUILD=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(EidoTest PRIVATE ${EIDO_LIBS} juce::juce_recommended_config_flags)
endif()
