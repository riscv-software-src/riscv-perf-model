project (mss)
add_library(mss
  BIU.cpp
  L2Cache.cpp
  MSS.cpp
  )
get_property(SPARTA_INCLUDE_PROP TARGET SPARTA::sparta PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(mss SYSTEM PRIVATE ${SPARTA_INCLUDE_PROP})
