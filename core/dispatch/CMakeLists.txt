add_library(dispatch
  Dispatch.cpp
  Dispatcher.cpp
)
target_include_directories(dispatch PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
target_link_libraries(dispatch PUBLIC core_inst core_cpu core_utils)
