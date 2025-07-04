cmake_minimum_required(VERSION 3.10)
project(TestApi)

# Include directories
include_directories(
    inc
    ../common
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/modules/cpm.mavis
)

# Source files
#file(GLOB ALL_SRC "src/*.cpp")
set(ALL_SRC
  src/ApiTestBench.cpp
  src/main.cpp
  src/FslTests.cpp
  src/TestData.cpp
  src/TestFieldExtractor.cpp
  src/Options.cpp
)

# Add executable
add_executable(test_api_exec ${ALL_SRC})
target_include_directories(test_api_exec 
    PRIVATE 
    ${CMAKE_SOURCE_DIR}/fsl/test/common 
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(test_api_exec fslinterp mavis boost_program_options)

set(ISA_FILES --isa_file ./json/isa_rv64g.json --isa_file ./json/isa_rv64c.json)


# Define the test target
add_custom_target(run_api_tests
    COMMAND test_api_exec
        --fsl_syntax_file ./fsl/test1.fsl
        --isa_file ./json/isa_rv64g.json
        --isa_file ./json/isa_rv64c.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running API tests"
)

# Ensure the necessary libraries and executable are built before running tests
add_dependencies(run_api_tests test_api_exec fslinterp mavis)
