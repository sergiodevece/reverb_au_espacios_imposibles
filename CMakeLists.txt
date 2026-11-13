cmake_minimum_required(VERSION 3.22)
project(ImpossibleSpaces VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(APPLE)
  option(EI_BUILD_PLUGIN "Build the JUCE plugin (AU + Standalone)" ON)
else()
  option(EI_BUILD_PLUGIN "Build the JUCE plugin (AU + Standalone)" OFF)
endif()
option(EI_BUILD_TESTS "Build the DSP tests" ON)

# ---- DSP: plain C++17, no JUCE ----
add_library(ei_dsp STATIC
  dsp/Fdn.cpp
  dsp/ReverbEngine.cpp)
target_include_directories(ei_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(ei_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
  target_compile_options(ei_dsp PRIVATE -Wall -Wextra)
endif()

if(EI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(EI_BUILD_PLUGIN)
  add_subdirectory(plugin)
endif()
