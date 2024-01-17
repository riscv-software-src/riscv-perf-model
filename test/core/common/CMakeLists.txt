
project(common_test)

add_library(common_test
  SourceUnit.cpp
  )
target_link_libraries(common_test PRIVATE core mavis SPARTA::sparta)
