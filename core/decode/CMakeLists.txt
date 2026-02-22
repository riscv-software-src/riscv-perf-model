add_library(decode
  FusionDecode.cpp
  Decode.cpp
  MavisUnit.cpp
)
target_include_directories(decode PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
target_link_libraries(decode PUBLIC core_inst core_cpu)
