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
    HRTFProcessor.cpp
    HRTFProcessor.h
    resources/resource.h
  RESOURCES
    resources/fonts/Roboto-Regular.ttf
)

set(SPATIALTAIL_LIBMYSOFA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libs/libmysofa/src")
set(SPATIALTAIL_LIBMYSOFA_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libs/libmysofa/build/src")
set(SPATIALTAIL_LIBMYSOFA_STATIC "${SPATIALTAIL_LIBMYSOFA_BUILD_DIR}/libmysofa.a")

set(SPATIALTAIL_PLUGIN_TARGETS
  ${PROJECT_NAME}-app
  ${PROJECT_NAME}-au
  ${PROJECT_NAME}-vst3
  ${PROJECT_NAME}AU-framework
  ${PROJECT_NAME}AUv3-appex
)

foreach(SPATIALTAIL_TARGET IN LISTS SPATIALTAIL_PLUGIN_TARGETS)
  if(TARGET ${SPATIALTAIL_TARGET})
    target_include_directories(${SPATIALTAIL_TARGET}
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${SPATIALTAIL_LIBMYSOFA_SRC_DIR}"
    )
    target_include_directories(${SPATIALTAIL_TARGET} SYSTEM PRIVATE "${SPATIALTAIL_LIBMYSOFA_BUILD_DIR}")

    if(EXISTS "${SPATIALTAIL_LIBMYSOFA_STATIC}")
      target_link_libraries(${SPATIALTAIL_TARGET} PRIVATE "${SPATIALTAIL_LIBMYSOFA_STATIC}" z)
    endif()
  endif()
endforeach()

include(CTest)
if(BUILD_TESTING)
  add_executable(reverb_stage_tests tests/reverb_stage_tests.cpp)
  target_include_directories(reverb_stage_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME reverb_stage_tests COMMAND reverb_stage_tests)

  # SOFA file used by hrtf_processor_tests. Falls back to the MIT KEMAR file
  # shipped with libmysofa. Override with -DHRTF_TEST_SOFA_PATH=/path/to/file.sofa
  if(NOT DEFINED HRTF_TEST_SOFA_PATH)
    set(HRTF_TEST_SOFA_PATH
      "${CMAKE_CURRENT_SOURCE_DIR}/../libs/libmysofa/share/MIT_KEMAR_normal_pinna.sofa")
  endif()

  add_executable(hrtf_processor_tests
    tests/hrtf_processor_tests.cpp
    HRTFProcessor.cpp
  )
  target_compile_features(hrtf_processor_tests PRIVATE cxx_std_17)
  target_include_directories(hrtf_processor_tests
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${SPATIALTAIL_LIBMYSOFA_SRC_DIR}"
  )
  target_include_directories(hrtf_processor_tests SYSTEM PRIVATE
    "${SPATIALTAIL_LIBMYSOFA_BUILD_DIR}"
  )
  target_compile_definitions(hrtf_processor_tests PRIVATE
    "HRTF_TEST_SOFA_PATH=\"${HRTF_TEST_SOFA_PATH}\""
  )
  if(EXISTS "${SPATIALTAIL_LIBMYSOFA_STATIC}")
    target_link_libraries(hrtf_processor_tests PRIVATE "${SPATIALTAIL_LIBMYSOFA_STATIC}" z)
  endif()
  add_test(NAME hrtf_processor_tests COMMAND hrtf_processor_tests)
endif()
