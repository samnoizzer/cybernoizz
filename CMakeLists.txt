cmake_minimum_required(VERSION 3.22)

project(CYBERNOIZZ VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Fetch the latest JUCE 8 automatically if not already present in a local subdirectory
include(FetchContent)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(CYBERNOIZZ
    COMPANY_NAME                "CyberNoizz Audio"
    COMPANY_COPYRIGHT           "CyberNoizz Audio"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    CyNz
    PLUGIN_CODE                 CyN1
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "CYBERNOIZZ"
)

juce_generate_juce_header(CYBERNOIZZ)

target_sources(CYBERNOIZZ
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

juce_add_binary_data(CYBERNOIZZ_BinaryData
    SOURCES
        Resources/IRs/01_Demanufacture_Bearsville_Recto412_V30_SM57.wav
        Resources/IRs/02_ZeroSignal_Marshall1960_G12T75_MD421.wav
        Resources/IRs/03_Replica_Hybrid_412_V30_57_421_Blend.wav
        Resources/IRs/04_NewBreed_Industrial_Scoop_412_SM57_OffAxis.wav
        Resources/IRs/05_Obsolete_EdgeCrusher_Oversized412_7String.wav
        Resources/IRs/06_SoulOfANewMachine_1992_LA_DeathThrash_412.wav
        Resources/IRs/07_Mechanize_Laboga_Custom412_V30_K100.wav
        Resources/IRs/08_Cyber_Tight_2x12_Studio_Precision_SM57_R121.wav
)

target_compile_definitions(CYBERNOIZZ
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

# High-performance CPU & SIMD optimization flags for minimum CPU hit
if(MSVC)
    target_compile_options(CYBERNOIZZ PRIVATE
        $<$<CONFIG:Release>:/O2 /Oi /Ot /arch:AVX2 /fp:fast /GL>
    )
    target_link_options(CYBERNOIZZ PRIVATE
        $<$<CONFIG:Release>:/LTCG>
    )
else()
    target_compile_options(CYBERNOIZZ PRIVATE
        $<$<CONFIG:Release>:-O3 -ffast-math -mavx2 -mfma -ftree-vectorize>
    )
endif()

target_link_libraries(CYBERNOIZZ
    PRIVATE
        CYBERNOIZZ_BinaryData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
