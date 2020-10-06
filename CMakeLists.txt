cmake_minimum_required(VERSION 3.16.0)

project(Chronometro VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(JUCE_MODULES
  juce_audio_basics
  juce_audio_devices
  juce_audio_formats
  juce_audio_processors
  juce_audio_utils
  juce_core
  juce_data_structures
  juce_dsp
  juce_events
  juce_graphics
  juce_gui_basics
  juce_gui_extra
  juce_opengl
)

if(NOT JUCE_ROOT_DIR)
  set(JUCE_ROOT_DIR "${PROJECT_SOURCE_DIR}/../JUCE")
endif()

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/juce-cmake/cmake")
find_package(JUCE REQUIRED COMPONENTS ${JUCE_MODULES})

set(SOURCES Source/Main.cpp Source/MainComponent.cpp JuceLibraryCode/BinaryData.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES
  MACOSX_BUNDLE true
  MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
  MACOSX_BUNDLE_BUNDLE_VERSION ${Chronometro_VERSION}
)
target_link_libraries(${PROJECT_NAME} ${JUCE_LIBRARIES})
source_group(Source FILES ${SOURCES})
