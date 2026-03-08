cmake_minimum_required(VERSION 3.14)
project(SpatialTail VERSION 1.0.0)

# Discover/repair IPLUG2_DIR for independent builds.
set(_needs_iplug2_discovery OFF)
if(NOT DEFINED IPLUG2_DIR)
  set(_needs_iplug2_discovery ON)
elseif(NOT EXISTS "${IPLUG2_DIR}/iPlug2.cmake")
  set(_needs_iplug2_discovery ON)
endif()

if(_needs_iplug2_discovery)
  foreach(CANDIDATE_DIR
      "${CMAKE_CURRENT_SOURCE_DIR}/../libs/iPlug2"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../iPlug2")
    if(EXISTS "${CANDIDATE_DIR}/iPlug2.cmake")
      set(IPLUG2_DIR "${CANDIDATE_DIR}" CACHE PATH "iPlug2 root directory" FORCE)
      break()
    endif()
  endforeach()
endif()

if(NOT DEFINED IPLUG2_DIR OR NOT EXISTS "${IPLUG2_DIR}/iPlug2.cmake")
  message(FATAL_ERROR "Unable to locate iPlug2.cmake. Set IPLUG2_DIR to your iPlug2 root.")
endif()
unset(_needs_iplug2_discovery)

# Include iPlug2 CMake configuration
include(${IPLUG2_DIR}/iPlug2.cmake)

# IGraphics backend/renderer can be set via command line:
#   cmake -DIGRAPHICS_BACKEND=SKIA -DIGRAPHICS_RENDERER=GL3 ..
# Defaults: NANOVG/METAL on macOS, NANOVG/GL2 on Windows
# See iPlug2/Scripts/cmake/IGraphics.cmake for all options

find_package(iPlug2 REQUIRED)

# Build all formats except AUv3 (AUv3 is opt-in)
# To include AUv3: FORMATS ALL_AUV3
# To select specific formats: FORMATS APP VST3 AU
iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    SpatialTail.cpp
    SpatialTail.h
    resources/resource.h
  RESOURCES
    resources/fonts/Roboto-Regular.ttf
)

include(CTest)
if(BUILD_TESTING)
  add_executable(reverb_stage_tests tests/reverb_stage_tests.cpp)
  target_include_directories(reverb_stage_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME reverb_stage_tests COMMAND reverb_stage_tests)
endif()
