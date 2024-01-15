cmake_minimum_required(VERSION 3.17)
project(FusionLibrary)

# Create an interface library
add_library(FusionLib INTERFACE)

target_include_directories(FusionLib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>  # for install
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mavis
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mavis/mavis
)
