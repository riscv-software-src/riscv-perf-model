add_library(fetch
  Fetch.cpp
  ICache.cpp
  SimpleBranchPred.cpp
)
target_link_libraries(fetch instgen)
