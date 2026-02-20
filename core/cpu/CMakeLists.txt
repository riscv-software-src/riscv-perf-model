add_library(core_cpu STATIC)

target_sources(core_cpu
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/CPU.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/CPUTopology.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/CPUFactories.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/CPUFactory.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/FlushManager.hpp
    PRIVATE
        CPU.cpp
        CPUTopology.cpp
        CPUFactory.cpp
)

target_include_directories(core_cpu PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(core_cpu PUBLIC core_sim_engine core_utils fetch core_rob core_preloader)
