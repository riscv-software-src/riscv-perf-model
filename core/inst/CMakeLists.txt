find_package(Boost REQUIRED COMPONENTS json)

add_library(core_inst STATIC
    Inst.cpp
    InstArchInfo.cpp
    InstGroup.cpp
    InstGenerator.cpp
)

target_sources(core_inst PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Inst.hpp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/InstAllocation.hpp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/InstArchInfo.hpp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/InstGenerator.hpp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/InstGroup.hpp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/LoadStoreInstInfo.hpp>
)

target_include_directories(core_inst PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(core_inst PRIVATE mavis Boost::json ${STF_LINK_LIBS} PUBLIC core_utils core_memory)