project (core)
add_library(core
  Core.cpp
  Inst.cpp
  InstArchInfo.cpp
  InstGroup.cpp
  InstGenerator.cpp
  ROB.cpp
  MMU.cpp
  Preloader.cpp
  CPU.cpp
  CPUFactory.cpp
  CPUTopology.cpp
)

get_property(SPARTA_INCLUDE_PROP TARGET SPARTA::sparta PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(core SYSTEM PRIVATE ${SPARTA_INCLUDE_PROP})

add_subdirectory(fetch)
add_subdirectory(decode)
add_subdirectory(dispatch)
add_subdirectory(execute)
add_subdirectory(vector)
add_subdirectory(lsu)

target_link_libraries(core fetch decode dispatch execute vector lsu)
