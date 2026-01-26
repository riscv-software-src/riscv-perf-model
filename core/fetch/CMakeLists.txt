add_library(fetch
  Fetch.cpp
  ICache.cpp
  SimpleBranchPred.cpp
  ../InstructionPrefetcher.cpp
  ../NextLinePrefetchEngine.cpp
  ../StridePrefetchEngine.cpp
)
target_link_libraries(fetch instgen)
