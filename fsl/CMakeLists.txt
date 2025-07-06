cmake_minimum_required(VERSION 3.10)

add_compile_options(-Wall -Werror)
find_package(FLEX 2.6 REQUIRED)
find_package(BISON 3.8 REQUIRED)

project(fsl)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CCSD ${CMAKE_CURRENT_SOURCE_DIR})
set(CBD  ${CMAKE_BINARY_DIR})

# Check if FSL_MAVIS_PATH is set
if(DEFINED FSL_MAVIS_PATH)
  set(FSL_MAVIS_DIR ${FSL_MAVIS_PATH})
else()
  set(FSL_MAVIS_DIR "${CCSD}/modules/cpm.mavis")
endif()

if(NOT TARGET mavis)
  add_subdirectory(${FSL_MAVIS_DIR})
endif()
  
# Include subdirectories
#add_subdirectory(modules/cpm.mavis)
add_subdirectory(fsl_interp)
add_subdirectory(fsl_api)
add_subdirectory(docs EXCLUDE_FROM_ALL)
#add_subdirectory(test)

# Custom target for building documentation
add_custom_target(docs
    COMMAND ${CMAKE_COMMAND} --build ${CBD}
                             --target docs_internal
    COMMENT "Generating all documentation"
)

add_dependencies(docs docs_internal)

# Find clang-tidy executable
find_program(CLANG_TIDY "clang-tidy")

# Ensure all source and header files are included
file(GLOB_RECURSE TIDY_FILES
    ${CCSD}/fsl_interp/src/*.cpp
    ${CCSD}/fsl_interp/inc/*.h
    ${CCSD}/fsl_api/*.cpp
    ${CCSD}/fsl_api/*.h
    # Add other directories or specific source files here
)

# Custom target for running clang-tidy and logging output
if(CLANG_TIDY)
  add_custom_target(clang-tidy
      COMMAND ${CLANG_TIDY} -p ${CBD} ${TIDY_FILES} > ${CBD}/clang-tidy.log 2>&1
      COMMENT "Running clang-tidy and log output in clang-tidy.log"
      WORKING_DIRECTORY ${CCSD}
  )
else()
  message(WARNING "clang-tidy not found!")
endif()
