cmake_minimum_required(VERSION 3.17)
project(FusionProject)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(FUSION_TOP ${CMAKE_SOURCE_DIR})

# Add subdirectories in dependent order
add_subdirectory(fusion)
add_subdirectory(dsl)
add_subdirectory(test)

# Check if Doxygen is installed
find_package(Doxygen)
if(DOXYGEN_FOUND)
    # Set input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxygen)

    # Request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    # Add a custom target to run Doxygen when user types "make docs"
    add_custom_target(docs
                      COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
                      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                      COMMENT "Generating API documentation with Doxygen"
                      VERBATIM)
else(DOXYGEN_FOUND)
  message("Doxygen needs to be installed to generate the doxygen documentation")
endif(DOXYGEN_FOUND)


