project (core)
add_library(core
  Core.cpp
  Fetch.cpp
  Decode.cpp
  Rename.cpp
  Dispatch.cpp
  Dispatcher.cpp
  Execute.cpp
  ExecutePipe.cpp
  Inst.cpp
  InstArchInfo.cpp
  InstGroup.cpp
  InstGenerator.cpp
  ROB.cpp
  LSU.cpp
  MavisUnit.cpp
  Preloader.cpp
  CPU.cpp
  CPUFactory.cpp
  CPUTopology.cpp
  )
target_include_directories(core SYSTEM PRIVATE ${Boost_INCLUDE_DIRS})
