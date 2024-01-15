cmake_minimum_required(VERSION 3.17)
project(FusionTest)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# FIXME: hard coded Specify the path to libmavis.a
set(MAVIS_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../release/mavis")

add_library(mavis STATIC IMPORTED)
set_target_properties(mavis PROPERTIES IMPORTED_LOCATION 
             "${MAVIS_LIB_PATH}/libmavis.a")

add_executable(fusiontest
               testbench.cpp
               testdata.cpp
               testfieldextractor.cpp
               fsltests.cpp
               main.cpp
               options.cpp )

target_link_libraries(fusiontest PRIVATE FusionLib FusionDSL mavis)


find_package(Boost REQUIRED COMPONENTS program_options)
if(Boost_FOUND)
    target_include_directories(fusiontest PRIVATE ${Boost_INCLUDE_DIRS})
    target_link_libraries(fusiontest 
                          PRIVATE FusionLib FusionDSL mavis
                          PRIVATE ${Boost_LIBRARIES} pthread )
else()
    message(SEND_ERROR "Boost is a required package for the testbench")
endif()

# FIXME: FSL <-> DSL is messy, rename ../dsl directory
set(JSON_BASE "${FUSION_TOP}/test/json")
set(FSL_BASE  "${FUSION_TOP}/test/fsl")

set(TB_OPTS --tb_verbose)

# There is a mavis required dependency for c after g
set(ISA_FILES
      --isa_file  ${JSON_BASE}/isa_rv64g.json
      --isa_file  ${JSON_BASE}/isa_rv64c.json
      --isa_file  ${JSON_BASE}/isa_rv64v.json)

set(FSL_FILES
      --fsl_file "${FSL_BASE}/test1.fsl"
      --fsl_file "${FSL_BASE}/test2.fsl")

set(FSL_SYNTAX_FILES
      --fsl_syntax_file "${FSL_BASE}/syntax1.fsl"
      --fsl_syntax_file "${FSL_BASE}/syntax2.fsl")

list(APPEND OPTS ${ISA_FILES} 
                 ${FSL_FILES} 
                 ${FSL_SYNTAX_FILES} 
                 ${TB_OPTS})

add_custom_target(regress
    COMMAND fusiontest ${OPTS}
    DEPENDS fusiontest
)
