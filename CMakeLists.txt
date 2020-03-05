cmake_minimum_required(VERSION 3.0.0)

project(chronometro)

set(LIBS
  juce_audio_basics
  juce_audio_devices
  juce_audio_formats
  juce_audio_processors
  juce_audio_utils
  juce_core
  juce_cryptography
  juce_data_structures
  juce_dsp
  juce_events
  juce_graphics
  juce_gui_basics
  juce_gui_extra
  juce_opengl
  juce_video
)

set(JUCE_ROOT_DIR "${PROJECT_SOURCE_DIR}/../JUCE")
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/juce-cmake/cmake")
find_package(JUCE REQUIRED COMPONENTS ${LIBS})
include_directories(${JUCE_INCLUDE_DIR})

set(SOURCES Source/Main.cpp Source/MainComponent.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE true)
target_link_libraries(${PROJECT_NAME} ${JUCE_LIBRARIES})
source_group(Source FILES ${SOURCES})
