add_library(core_rob STATIC
    ROB.cpp
)

target_sources(core_rob PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ROB.hpp>
)

target_include_directories(core_rob PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(core_rob PUBLIC core_utils core_inst core_cpu)