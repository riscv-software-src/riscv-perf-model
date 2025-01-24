add_library(dispatch
  Dispatch.cpp
  Dispatcher.cpp
)
target_link_libraries(dispatch instgen)
