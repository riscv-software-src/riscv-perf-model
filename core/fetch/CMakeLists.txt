add_library(fetch
  Fetch.cpp
  ICache.cpp
  EnhancedBranchPred.cpp
  SimpleBranchPred.cpp
)
target_link_libraries(fetch instgen)
