add_library(core_preloader STATIC
    Preloader.cpp
)

target_sources(core_preloader PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Preloader.hpp>
)

target_include_directories(core_preloader PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)