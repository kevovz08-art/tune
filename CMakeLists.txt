cmake_minimum_required(VERSION 3.22)
project(RelapseTune VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
if(DEFINED ENV{CI})
  set(RT_COPY FALSE)
else()
  set(RT_COPY TRUE)
endif()

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(RelapseTune
  COMPANY_NAME "RelapseAudio"
  PLUGIN_MANUFACTURER_CODE Rlps
  PLUGIN_CODE Rtun
  FORMATS VST3
  PRODUCT_NAME "RelapseTune"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD ${RT_COPY}
  VST3_CATEGORIES Fx Pitch Shift)

target_sources(RelapseTune PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginProcessor.h
  Source/PitchEngine.h
  Source/PluginEditor.h)

target_compile_definitions(RelapseTune PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(RelapseTune
  PRIVATE juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags
         juce::juce_recommended_lto_flags
         juce::juce_recommended_warning_flags)
