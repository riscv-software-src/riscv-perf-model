cmake_minimum_required(VERSION 3.10)

# Project name
project(fsl_interp)

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include directories
include_directories(inc ${CMAKE_CURRENT_BINARY_DIR})

# Find Boost
find_package(Boost REQUIRED COMPONENTS program_options)
include_directories(${Boost_INCLUDE_DIRS})

# Bison and Flex files
set(BISON_SRC src/fsl.y)
set(FLEX_SRC src/fsl.l)
set(BISON_OUT ${CMAKE_CURRENT_BINARY_DIR}/yy.tab.cpp)
set(BISON_H ${CMAKE_CURRENT_BINARY_DIR}/yy.tab.hpp)
set(FLEX_OUT ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cpp)

# Bison and Flex targets
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

BISON_TARGET(Parser ${BISON_SRC} ${BISON_OUT} DEFINES_FILE ${BISON_H})
FLEX_TARGET(Lexer ${FLEX_SRC} ${FLEX_OUT})
ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

# Source files
file(GLOB ALL_SRC "src/*.cpp")
list(REMOVE_ITEM ALL_SRC ${BISON_SRC} ${FLEX_SRC}) # Exclude .l and .y from source list

# Library target
add_library(fslinterp STATIC ${ALL_SRC} ${BISON_OUT} ${FLEX_OUT})

# Set the output name for the library
set_target_properties(fslinterp PROPERTIES OUTPUT_NAME "fsl_interp")

# Executable target
add_executable(fsl_interp ${ALL_SRC} ${BISON_OUT} ${FLEX_OUT})
target_link_libraries(fsl_interp fslinterp ${Boost_LIBRARIES})

# Custom target for cleaning
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/Makefile
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/cmake_install.cmake
    COMMENT "Cleaning all generated files"
)

# Add dependencies for the executable
add_dependencies(fsl_interp fslinterp)


#cmake_minimum_required(VERSION 3.10)
#
## Project name
#project(fsl_interp)
#
## Set output directories
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
#
## Include directories
#include_directories(inc ${CMAKE_CURRENT_BINARY_DIR})
#
## Find Boost
#find_package(Boost REQUIRED COMPONENTS program_options)
#include_directories(${Boost_INCLUDE_DIRS})
#
## Bison and Flex files
#set(BISON_SRC src/fsl.y)
#set(FLEX_SRC src/fsl.l)
#set(BISON_OUT ${CMAKE_CURRENT_BINARY_DIR}/yy.tab.cpp)
#set(BISON_H ${CMAKE_CURRENT_BINARY_DIR}/yy.tab.hpp)
#set(FLEX_OUT ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cpp)
#
## Bison and Flex targets
#find_package(BISON REQUIRED)
#find_package(FLEX REQUIRED)
#
#BISON_TARGET(Parser ${BISON_SRC} ${BISON_OUT} DEFINES_FILE ${BISON_H})
#FLEX_TARGET(Lexer ${FLEX_SRC} ${FLEX_OUT})
#ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)
#
## Source files
#file(GLOB ALL_SRC "src/*.cpp")
#list(REMOVE_ITEM ALL_SRC ${BISON_SRC} ${FLEX_SRC}) # Exclude .l and .y from source list
#
## Library target
#add_library(fslinterp STATIC ${ALL_SRC} ${BISON_OUT} ${FLEX_OUT})
#
## Set the output name for the library
#set_target_properties(fslinterp PROPERTIES OUTPUT_NAME "fsl_interp")
#
## Executable target
#add_executable(fslinterp_exe ${ALL_SRC} ${BISON_OUT} ${FLEX_OUT})
#target_link_libraries(fslinterp_exe fslinterp ${Boost_LIBRARIES})
#
## Custom target for cleaning
#add_custom_target(clean-all
#    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/bin
#    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/lib
#    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
#    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
#    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/Makefile
#    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/cmake_install.cmake
#    COMMENT "Cleaning all generated files"
#)
#
## Add dependencies for the executable
#add_dependencies(fslinterp_exe fslinterp)
#
#
##cmake_minimum_required(VERSION 3.10)
##
### Project name
##project(fsl_interp)
##
### Set output directories
##set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
##set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
##set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
##
### Include directories
##include_directories(inc ${CMAKE_CURRENT_BINARY_DIR})
##
### Find Boost
##find_package(Boost REQUIRED COMPONENTS program_options)
##include_directories(${Boost_INCLUDE_DIRS})
##
### Bison and Flex files
##set(BISON_SRC src/fsl.y)
##set(FLEX_SRC src/fsl.l)
##set(BISON_OUT ${CMAKE_CURRENT_BINARY_DIR}/yy.tab.cpp)
##set(BISON_H ${CMAKE_CURRENT_BINARY_DIR}/yy.tab.hpp)
##set(FLEX_OUT ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cpp)
##
### Bison and Flex targets
##find_package(BISON REQUIRED)
##find_package(FLEX REQUIRED)
##
##BISON_TARGET(Parser ${BISON_SRC} ${BISON_OUT} DEFINES_FILE ${BISON_H})
##FLEX_TARGET(Lexer ${FLEX_SRC} ${FLEX_OUT})
##ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)
##
### Source files
##file(GLOB ALL_SRC "src/*.cpp")
##list(REMOVE_ITEM ALL_SRC ${BISON_SRC} ${FLEX_SRC}) # Exclude .l and .y from source list
##
### Library target
##add_library(fslinterp STATIC ${ALL_SRC} ${BISON_OUT} ${FLEX_OUT})
##
### Executable target
##add_executable(fslinterp_exe ${ALL_SRC} ${BISON_OUT} ${FLEX_OUT})
##target_link_libraries(fslinterp_exe fslinterp ${Boost_LIBRARIES})
##
### Custom target for cleaning
##add_custom_target(clean-all
##    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/bin
##    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/lib
##    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
##    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
##    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/Makefile
##    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/cmake_install.cmake
##    COMMENT "Cleaning all generated files"
##)
##
### Add dependencies for the executable
##add_dependencies(fslinterp_exe fslinterp)
##
