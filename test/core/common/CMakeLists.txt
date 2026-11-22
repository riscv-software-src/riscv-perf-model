
project(common_test)

add_library(common_test
  SourceUnit.cpp
  )
target_link_libraries(common_test PUBLIC
  core instgen mavis SPARTA::sparta olympia_common olympia_options
)
target_include_directories(common_test PUBLIC ${SIM_BASE})

add_library(olympia_test_support INTERFACE)
target_link_libraries(olympia_test_support INTERFACE common_test)
