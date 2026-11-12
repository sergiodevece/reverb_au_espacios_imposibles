# JUCE plugin target. Skipped entirely when EI_BUILD_PLUGIN=OFF.
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
  set(EI_FORMATS AU Standalone)
else()
  set(EI_FORMATS Standalone)   # compile check only; the product is the macOS Audio Unit
endif()
option(EI_COPY_PLUGIN "Install the built plugin into the user plug-in folder" ${APPLE})

juce_add_plugin(ImpossibleSpaces
  PRODUCT_NAME "Impossible Spaces"
  COMPANY_NAME "VitaPlug"
  BUNDLE_ID com.vitaplug.impossiblespaces
  PLUGIN_MANUFACTURER_CODE Vita
  PLUGIN_CODE ImSp
  FORMATS ${EI_FORMATS}
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  AU_MAIN_TYPE kAudioUnitType_Effect
  COPY_PLUGIN_AFTER_BUILD ${EI_COPY_PLUGIN})

juce_add_binary_data(ImpossibleSpacesAssets SOURCES
  assets/fonts/Michroma-Regular.ttf
  assets/fonts/ChakraPetch-Medium.ttf
  assets/fonts/ChakraPetch-SemiBold.ttf
  assets/fonts/JetBrainsMono-Regular.ttf)
set_target_properties(ImpossibleSpacesAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(ImpossibleSpaces PRIVATE
  Parameters.cpp
  PluginProcessor.cpp
  PluginEditor.cpp
  ui/EnvelopeDisplay.cpp
  ui/EqDisplay.cpp
  ui/LfoView.cpp)

target_include_directories(ImpossibleSpaces PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(ImpossibleSpaces PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0)

target_link_libraries(ImpossibleSpaces
  PRIVATE
    ei_dsp
    ImpossibleSpacesAssets
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)
