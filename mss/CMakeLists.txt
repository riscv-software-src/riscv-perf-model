project (mss)
add_library(mss
  BIU.cpp
  L2Cache.cpp
  MSS.cpp
  )
target_link_libraries(mss PUBLIC instruction)
