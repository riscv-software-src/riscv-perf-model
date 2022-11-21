cmake_minimum_required(VERSION 3.16)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

project(olympia CXX)

add_compile_options ( -Werror
  -Wall -Wextra -Winline -Winit-self -Wno-unused-function
  -Wuninitialized -Wno-sequence-point -Wno-inline -Wno-unknown-pragmas
  -Woverloaded-virtual -Wno-unused-parameter -Wno-missing-field-initializers -pipe)

################################################################################
# Set up Sparta
if (NOT SPARTA_BASE)
  message (FATAL_ERROR "Need to define the location of the Sparta: cmake <args> -DSPARTA_BASE=\"/path/to/sparta\"")
endif ()
include (${SPARTA_BASE}/cmake/sparta-config.cmake)

################################################################################
# Set up STF library
set (STF_LIB_BASE ${PROJECT_SOURCE_DIR}/stf_lib)
set (DISABLE_STF_DOXYGEN ON)
include(${STF_LIB_BASE}/cmake/stf-config.cmake)

# Use ccache if installed
find_program (CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
  set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
  message ("-- Using ccache")
endif ()

set (SIM_BASE ${PROJECT_SOURCE_DIR})
add_executable(olympia
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
set(CMAKE_CXX_FLAGS_FASTDEBUG "-O3 -g")

# Remove -pedantic from the STF compiler options -- issues with clang
# on MacOS and variatic macros (like SPARTA_ADDPAIR)
if (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
  get_target_property(_target_cxx_flags olympia COMPILE_OPTIONS)
  list(REMOVE_ITEM _target_cxx_flags -pedantic)
  set_target_properties(olympia PROPERTIES COMPILE_OPTIONS "${_target_cxx_flags}")
endif()

# Include directories
include_directories (core mss)
include_directories (SYSTEM mavis)
include_directories (SYSTEM stf_lib)
include_directories (SYSTEM ${SPARTA_BASE})
include_directories (SYSTEM ${SPARTA_BASE}/simdb/include)

# The Core and MSS
add_subdirectory(core)
add_subdirectory(mss)

# Add STF library to the build
add_subdirectory (${STF_LIB_BASE})

# Add testing, but do not build as part of the 'all' target
add_subdirectory (test EXCLUDE_FROM_ALL)

# Sparta/STF linking
target_link_directories (olympia PUBLIC
  ${SPARTA_BASE}/${SPARTA_BUILD_TYPE}
  ${SPARTA_BASE}/${SPARTA_BUILD_TYPE}/simdb
  )

target_link_libraries (olympia core mss ${Sparta_LIBS} ${STF_LINK_LIBS})

# Create a few links like reports and arch directories
file(CREATE_LINK ${PROJECT_SOURCE_DIR}/reports    ${CMAKE_CURRENT_BINARY_DIR}/reports         SYMBOLIC)
file(CREATE_LINK ${PROJECT_SOURCE_DIR}/arches     ${CMAKE_CURRENT_BINARY_DIR}/arches          SYMBOLIC)
file(CREATE_LINK ${PROJECT_SOURCE_DIR}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${PROJECT_SOURCE_DIR}/traces     ${CMAKE_CURRENT_BINARY_DIR}/traces          SYMBOLIC)
