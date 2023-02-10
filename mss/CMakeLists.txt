project (mss)
add_library(mss
  BIU.cpp
  MSS.cpp
  )
target_include_directories(mss SYSTEM PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(mss PUBLIC SPARTA::sparta)
