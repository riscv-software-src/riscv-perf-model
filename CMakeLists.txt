cmake_minimum_required(VERSION 3.16)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

project(olympia CXX)

add_compile_options ( -Werror
  -Wall -Wextra -Winline -Winit-self -Wno-unused-function
  -Wuninitialized -Wno-sequence-point -Wno-inline -Wno-unknown-pragmas
  -Woverloaded-virtual -Wno-unused-parameter -Wno-missing-field-initializers -pipe)

if (NOT SPARTA_BASE)
  message (FATAL_ERROR "Need to define the location of the Sparta: cmake <args> -DSPARTA_BASE=\"/path/to/sparta\"")
endif ()
include (${SPARTA_BASE}/cmake/sparta-config.cmake)

# Use ccache if we've got it.
find_program (CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
  set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
  message ("-- Using ccache")
endif ()

set (SIM_BASE ${PROJECT_SOURCE_DIR})
add_executable(olympia
  core/Core.cpp
  core/Fetch.cpp
  core/Decode.cpp
  core/Rename.cpp
  core/Dispatch.cpp
  core/Execute.cpp
  core/Inst.cpp
  core/InstGroup.cpp
  core/InstGenerator.cpp
  core/ROB.cpp
  core/LSU.cpp
  core/MavisUnit.cpp
  core/Preloader.cpp
  core/CPU.cpp
  core/CPUFactory.cpp
  core/CPUTopology.cpp
  mss/BIU.cpp
  mss/MSS.cpp
  sim/OlympiaSim.cpp
  sim/main.cpp
  )


if (CMAKE_BUILD_TYPE MATCHES "^[Rr]elease")
  set(SPARTA_BUILD_TYPE "release")
  target_link_options    (olympia PUBLIC -flto)
  target_compile_options (olympia PUBLIC -flto)
elseif (CMAKE_BUILD_TYPE MATCHES "^[Ff]ast[Dd]ebug")
  set(SPARTA_BUILD_TYPE "release")
elseif (CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
  set(SPARTA_BUILD_TYPE "debug")
elseif (CMAKE_BUILD_TYPE MATCHES "^[Pp]rofile")
  set(SPARTA_BUILD_TYPE "release")
else()
  message (FATAL_ERROR "Please provide a CMAKE_BUILD_TYPE: -DCMAKE_BUILD_TYPE=Release|Debug|Profile")
endif()

# Profile build flags
set(CMAKE_CXX_FLAGS_PROFILE "-O3 -pg -g -ftime-trace")
set(CMAKE_CXX_FLAGS_FASTDEBUG "-g")

# Include directories
include_directories (core mss mavis)
include_directories (SYSTEM ${SPARTA_BASE})
include_directories (SYSTEM ${SPARTA_BASE}/simdb/include)

# Sparta linking
target_link_directories (olympia PUBLIC
  ${SPARTA_BASE}/${SPARTA_BUILD_TYPE}
  ${SPARTA_BASE}/${SPARTA_BUILD_TYPE}/simdb
  )
target_link_libraries (olympia ${Sparta_LIBS})

# Create a few links like reports and arch directories
file(CREATE_LINK ${PROJECT_SOURCE_DIR}/reports    ${CMAKE_CURRENT_BINARY_DIR}/reports         SYMBOLIC)
file(CREATE_LINK ${PROJECT_SOURCE_DIR}/arches     ${CMAKE_CURRENT_BINARY_DIR}/arches          SYMBOLIC)
file(CREATE_LINK ${PROJECT_SOURCE_DIR}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)

# Add testing, but do not build as part of the 'all' target
add_subdirectory (test EXCLUDE_FROM_ALL)
