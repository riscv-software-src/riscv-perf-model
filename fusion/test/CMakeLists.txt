cmake_minimum_required(VERSION 3.17)
project(FusionTest)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# FIXME: hard coded Specify the path to libmavis.a
set(MAVIS_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../release/mavis")

add_library(mavis STATIC IMPORTED)
set_target_properties(mavis PROPERTIES IMPORTED_LOCATION 
             "${MAVIS_LIB_PATH}/libmavis.a")

add_executable(fusiontest
               TestBench.cpp
               TestData.cpp
               TestFieldExtractor.cpp
#               FslTests.cpp
               main.cpp
               Options.cpp )

#target_link_libraries(fusiontest PRIVATE FusionLib FusionDSL mavis)


find_package(Boost REQUIRED COMPONENTS program_options)
if(Boost_FOUND)
    target_include_directories(fusiontest PRIVATE ${Boost_INCLUDE_DIRS})
    target_link_libraries(fusiontest 
                          PRIVATE FusionLib mavis
                          PRIVATE ${Boost_LIBRARIES} pthread )
else()
    message(SEND_ERROR "Boost is a required package for the testbench")
endif()

set(JSON_BASE "${FUSION_TOP}/test/json")

set(TB_OPTS --tb_verbose)

# There is a mavis required dependency for c after g
set(ISA_FILES
      --isa_file  ${JSON_BASE}/isa_rv64g.json
      --isa_file  ${JSON_BASE}/isa_rv64c.json)

list(APPEND OPTS ${ISA_FILES} 
                 ${TB_OPTS})

add_custom_target(regress
    COMMAND fusiontest ${OPTS}
)
