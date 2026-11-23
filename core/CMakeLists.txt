project (core)

# Generate git version header at build time
set(GIT_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/git_version.h")

add_custom_command(
    OUTPUT ${GIT_VERSION_FILE}
    COMMAND ${CMAKE_COMMAND}
        -DGIT_VERSION_FILE=${GIT_VERSION_FILE}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/GenerateGitVersion.cmake
    DEPENDS
        ${CMAKE_SOURCE_DIR}/.git/HEAD
        ${CMAKE_SOURCE_DIR}/.git/index
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating git version information"
    VERBATIM
)

# Create a custom target that depends on the generated header
add_custom_target(git_version_target DEPENDS ${GIT_VERSION_FILE})

add_library(core
  Core.cpp
  ROB.cpp
  MMU.cpp
  Preloader.cpp
  CPU.cpp
  CPUFactory.cpp
  CPUTopology.cpp
)

add_library(instruction
  Inst.cpp
  InstArchInfo.cpp
  InstGroup.cpp
  FlushManager.cpp
  LoadStoreInstInfo.cpp
  MemoryAccessInfo.cpp
)

add_library(instgen
  InstGenerator.cpp
)

find_package(Boost REQUIRED COMPONENTS json)

target_link_libraries(instruction PUBLIC stf mavis Boost::json)
target_link_libraries(instgen PUBLIC instruction)

# Make core library depend on git_version_target
add_dependencies(core git_version_target)
target_include_directories(core PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(fetch)
add_subdirectory(decode)
add_subdirectory(rename)
add_subdirectory(dispatch)
add_subdirectory(execute)
add_subdirectory(vector)
add_subdirectory(lsu)

target_link_libraries(core
  PUBLIC fetch decode rename dispatch execute vector lsu instruction
)
if(EDM_ENABLED)
  add_subdirectory(edm)
  target_link_libraries(instgen PUBLIC edm)
endif()

