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

add_subdirectory(cpu)
add_subdirectory(inst)
add_subdirectory(memory)
add_subdirectory(rob)
add_subdirectory(preloader)
add_subdirectory(utils)
add_subdirectory(sim_engine)
add_subdirectory(fetch)
add_subdirectory(decode)
add_subdirectory(rename)
add_subdirectory(dispatch)
add_subdirectory(execute)
add_subdirectory(vector)
add_subdirectory(lsu)

target_include_directories(core_sim_engine PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(core_sim_engine git_version_target)
