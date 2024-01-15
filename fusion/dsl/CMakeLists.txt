cmake_minimum_required(VERSION 3.17)
project(FusionDSL)

# Find required packages
find_package(BISON)
find_package(FLEX)

# Check if BISON and FLEX are found
if(NOT BISON_FOUND)
    message(SEND_ERROR "Bison not found")
endif()
if(NOT FLEX_FOUND)
    message(SEND_ERROR "Flex not found")
endif()

# Bison and Flex targets
BISON_TARGET(FusionParser fsl.y ${CMAKE_CURRENT_BINARY_DIR}/yy.tab.cpp VERBOSE)
FLEX_TARGET(FusionScanner fsl.l ${CMAKE_CURRENT_BINARY_DIR}/yy.lex.cpp)

ADD_FLEX_BISON_DEPENDENCY(FusionScanner FusionParser)

# Include the binary directory to include the generated parser.h file
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(PARSER_SOURCES 
    qparser.cpp
    ${BISON_FusionParser_OUTPUTS}
    ${FLEX_FusionScanner_OUTPUTS}
)

add_library(FusionDSL STATIC ${PARSER_SOURCES})

target_link_libraries(FusionDSL PUBLIC FusionLib)

target_include_directories(FusionDSL PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/test
)
