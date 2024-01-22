cmake_minimum_required(VERSION 3.17)
project(FusionProject)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(FUSION_TOP ${CMAKE_SOURCE_DIR})

# Add subdirectories in dependent order
add_subdirectory(fusion)
add_subdirectory(dsl)
add_subdirectory(test)

# Copy the symtab_expect.txt file
set(SYM_EXPECT_SRC ${CMAKE_SOURCE_DIR}/test/expect/symtab_expect.txt)
set(SYM_EXPECT_DST ${CMAKE_BINARY_DIR}/test/symtab_expect.txt)

add_custom_command(
    OUTPUT ${SYM_EXPECT_DST}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SYM_EXPECT_SRC} ${SYM_EXPECT_DST}
    DEPENDS ${SYM_EXPECT_SRC}
    COMMENT "Copying symtab_expect.txt to build/test directory"
)

add_custom_target(copy_symtab_expect ALL
    DEPENDS ${SYM_EXPECT_DST}
)

find_package(Doxygen)
if(DOXYGEN_FOUND)
    # Set input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxygen)

    # Request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    # Add a custom target for "make docs"
    add_custom_target(docs
                      COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
                      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                      COMMENT "Generating API documentation with Doxygen"
                      VERBATIM)
else()
    message("Doxygen needs to be installed to generate documentation")
endif()

