cmake_minimum_required(VERSION 3.25)
project(RVRSE-OOS)

set(IPLUG2_DIR ${CMAKE_SOURCE_DIR}/iPlug2)
include(${IPLUG2_DIR}/iPlug2.cmake)

find_package(iPlug2 REQUIRED)

# signalsmith-stretch: MIT-licensed spectral time-stretcher (brings signalsmith-linear via FetchContent)
include(FetchContent)
FetchContent_Declare(
  signalsmith-stretch
  GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
  GIT_TAG        57b93f4e9206a089a45387eaa39bdc9f310d3308  # main @ 2026-04 (v1.3.2)
  GIT_SHALLOW    OFF
)
FetchContent_MakeAvailable(signalsmith-stretch)

# dr_mp3 is a single-header permissively licensed decoder (dr_libs / public domain or MIT-0).
# Download during configure if the header is not already vendored.
set(RVRSE_MP3_HEADER "${CMAKE_SOURCE_DIR}/RVRSE/libs/dr_libs/dr_mp3.h")
if(NOT EXISTS "${RVRSE_MP3_HEADER}")
  file(DOWNLOAD
    "https://raw.githubusercontent.com/mackron/dr_libs/master/dr_mp3.h"
    "${RVRSE_MP3_HEADER}"
    STATUS _mp3_download_status
    TLS_VERIFY ON)
  list(GET _mp3_download_status 0 _mp3_status_code)
  if(NOT _mp3_status_code EQUAL 0)
    message(FATAL_ERROR "Could not fetch dr_mp3.h. Download it manually from mackron/dr_libs into RVRSE/libs/dr_libs/.")
  endif()
endif()

add_subdirectory(RVRSE)

# Unit tests (Catch2) — opt-in via BUILD_TESTING (ON by default)
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
