add_library(lsu
  DCache.cpp
  LSU.cpp
)
target_link_libraries(lsu PUBLIC instruction)
