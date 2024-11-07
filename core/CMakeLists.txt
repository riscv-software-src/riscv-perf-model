project (core)
add_library(core
  FusionDecode.cpp
  Core.cpp
  SimpleBranchPred.cpp
  ICache.cpp
  Fetch.cpp
  Decode.cpp
  VectorUopGenerator.cpp
  Rename.cpp
  Dispatch.cpp
  Dispatcher.cpp
  Execute.cpp
  ExecutePipe.cpp
  Inst.cpp
  InstArchInfo.cpp
  InstGroup.cpp
  InstGenerator.cpp
  IssueQueue.cpp
  ROB.cpp
  LSU.cpp
  VLSU.cpp
  MMU.cpp
  DCache.cpp
  MavisUnit.cpp
  Preloader.cpp
  CPU.cpp
  CPUFactory.cpp
  CPUTopology.cpp
)
get_property(SPARTA_INCLUDE_PROP TARGET SPARTA::sparta PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(core SYSTEM PRIVATE ${SPARTA_INCLUDE_PROP})
