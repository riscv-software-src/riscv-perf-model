add_library(fetch
  Fetch.cpp
  ICache.cpp
  SimpleBranchPred.cpp
  BasePredictor.cpp
  TAGE_SC_L.cpp
  BPU.cpp
)
target_link_libraries(fetch instgen)
