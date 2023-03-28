
project(common_test)

add_library(common_test
  SourceUnit.cpp
  )
target_include_directories(common_test SYSTEM PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(common_test SPARTA::sparta)
