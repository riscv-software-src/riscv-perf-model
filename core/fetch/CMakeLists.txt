add_library(fetch
  Fetch.cpp
  ICache.cpp
  SimpleBranchPred.cpp
)
target_link_libraries(fetch PUBLIC core_inst core_utils core_memory core_cpu)
