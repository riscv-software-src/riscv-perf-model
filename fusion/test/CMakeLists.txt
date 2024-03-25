cmake_minimum_required(VERSION 3.17)
project(FusionTest)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_library(MAVIS_LIBRARY NAMES mavis
             HINTS "${CMAKE_SOURCE_DIR}/../release/mavis"
                   "${CMAKE_SOURCE_DIR}/../debug/mavis"
                   "${CMAKE_SOURCE_DIR}/../fastdebug/mavis"
                   "${CMAKE_SOURCE_DIR}/../build/mavis")

if(NOT MAVIS_LIBRARY)
  message(FATAL_ERROR "libmavis.a not found")
endif()

add_library(mavis STATIC IMPORTED)

set_target_properties(mavis PROPERTIES IMPORTED_LOCATION 
             ${MAVIS_LIBRARY})

add_executable(fusiontest
               TestBench.cpp
               TestData.cpp
               TestFieldExtractor.cpp
               FslTests.cpp
               main.cpp
               Options.cpp )

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

set(JSON_BASE "${FUSION_TOP}/test/json")
set(FSL_BASE  "${FUSION_TOP}/test/fsl")

set(TB_OPTS --tb_verbose)

# There is a mavis required dependency for c after g
set(ISA_FILES
      --isa_file  ${JSON_BASE}/isa_rv64g.json
      --isa_file  ${JSON_BASE}/isa_rv64c.json)

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
    DEPENDS fusiontest copy_symtab_expect
)
