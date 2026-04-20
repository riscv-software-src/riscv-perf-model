add_library(core_utils INTERFACE)

target_sources(core_utils PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/CoreExtensions.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CoreTypes.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CoreUtils.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/MiscUtils.hpp
)