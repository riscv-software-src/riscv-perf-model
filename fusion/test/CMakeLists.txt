cmake_minimum_required(VERSION 3.17)
project(FusionTest)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Boost REQUIRED COMPONENTS program_options)

# Specify the path to libmavis.a
set(MAVIS_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../release/mavis")

# Create an imported target for libmavis
add_library(mavis STATIC IMPORTED)
set_target_properties(mavis PROPERTIES IMPORTED_LOCATION "${MAVIS_LIB_PATH}/libmavis.a")

# Add the executable
add_executable(fusiontest
               testbench.cpp
               main.cpp
               options.cpp )

target_include_directories(fusiontest PRIVATE
       ${CMAKE_SOURCE_DIR}/./dsl
       ${CMAKE_SOURCE_DIR}/../mavis
       ${CMAKE_SOURCE_DIR}/../mavis/mavis)

# If Boost was found, link the libraries to the executable
if(Boost_FOUND)
    target_include_directories(fusiontest PRIVATE ${Boost_INCLUDE_DIRS})
    target_link_libraries(fusiontest FusionLib ${Boost_LIBRARIES} mavis)
endif()

set(TB_OPTS --tb_verbose)

list(APPEND OPTS ${ISA_FILES} ${TB_OPTS})

add_custom_target(regress
    COMMAND fusiontest ${OPTS}
    DEPENDS fusiontest
)



#cmake_minimum_required(VERSION 3.17)
#project(FusionTest)
#
#find_package(Boost REQUIRED COMPONENTS program_options)
#
#set(MAVIS_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../release/mavis")
#
#add_library(mavis STATIC IMPORTED)
#set_target_properties(mavis 
#           PROPERTIES IMPORTED_LOCATION 
#           "${MAVIS_LIB_PATH}/libmavis.a")
#
## Add the executable
#add_executable(fusiontest 
#               testbench.cpp
#               main.cpp
#               options.cpp )
#
#
#target_include_directories(fusiontest PRIVATE 
#       ${CMAKE_SOURCE_DIR}/./dsl
#       ${CMAKE_SOURCE_DIR}/../mavis
#       ${CMAKE_SOURCE_DIR}/../mavis/mavis)
#
## If Boost was found, link the libraries to the executable
#if(Boost_FOUND)
#    target_include_directories(fusiontest PRIVATE ${Boost_INCLUDE_DIRS})
#    target_link_libraries(fusiontest FusionLib mavis ${Boost_LIBRARIES})
#endif()
#
##set(ISA_FILES
##      --isa_file ../../../mavis/json/isa_rv64g.json
##      --isa_file ../../../mavis/json/isa_rv64c.json)
#
#set(TB_OPTS --tb_verbose)
#
##set(OPTS)
#list(APPEND OPTS ${ISA_FILES} ${TB_OPTS})
#
#add_custom_target(regress
#    COMMAND fusiontest ${OPTS}
#    DEPENDS fusiontest
#)
