project (core)

# Get the current git commit hash and dirty status
execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()

add_definitions(-DGIT_COMMIT_HASH="${GIT_COMMIT_HASH}")

add_library(core
  Core.cpp
  ROB.cpp
  MMU.cpp
  Preloader.cpp
  CPU.cpp
  CPUFactory.cpp
  CPUTopology.cpp
)

add_library(instgen
  Inst.cpp
  InstArchInfo.cpp
  InstGroup.cpp
  InstGenerator.cpp
)

find_package(Boost REQUIRED COMPONENTS json)

target_link_libraries(instgen stf zstd mavis Boost::json)

get_property(SPARTA_INCLUDE_PROP TARGET SPARTA::sparta PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(core SYSTEM PRIVATE ${SPARTA_INCLUDE_PROP})

add_subdirectory(fetch)
add_subdirectory(decode)
add_subdirectory(rename)
add_subdirectory(dispatch)
add_subdirectory(execute)
add_subdirectory(vector)
add_subdirectory(lsu)

target_link_libraries(core fetch decode rename dispatch execute vector lsu instgen)
