cmake_minimum_required(VERSION 3.17)
project(FusionProject)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(FUSION_TOP ${CMAKE_SOURCE_DIR})

add_subdirectory(fusion)
add_subdirectory(test)

find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxygen)

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(docs
                      COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
                      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                      COMMENT "Generating API documentation with Doxygen"
                      VERBATIM)
else()
    message("Doxygen needs to be installed to generate documentation")
endif()

