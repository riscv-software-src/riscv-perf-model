
project(common_test)

add_library(common_test
  SourceUnit.cpp
  )
target_include_directories(common_test
    PUBLIC ${PROJECT_SOURCE_DIR}/..
)
target_link_libraries(common_test PRIVATE core_inst mavis SPARTA::sparta)
