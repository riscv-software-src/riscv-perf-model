cmake_minimum_required(VERSION 3.10)
project(fsl_docs)

set(DOXY doxygen)
set(CCBD ${CMAKE_CURRENT_BINARY_DIR})
set(CCSD ${CMAKE_CURRENT_SOURCE_DIR})

# Path to the Doxyfile configurations
set(DOXYFILE_INTERP ${CCSD}/DoxyInterp)
set(DOXYFILE_API ${CCSD}/DoxyApi)


# Define the docs_interp target
add_custom_target(docs_interp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CCBD}/docs_interp
    COMMAND ${DOXY} ${DOXYFILE_INTERP}
    WORKING_DIRECTORY ${CCSD}
    COMMENT "Generating documentation for interp"
)

# Define the docs_api target
add_custom_target(docs_api
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CCBD}/docs_api
    COMMAND ${DOXY} ${DOXYFILE_API}
    WORKING_DIRECTORY ${CCSD}
    COMMENT "Generating documentation for API"
)

# Define the docs_internal target that depends on docs_interp and docs_api
add_custom_target(docs_internal
    DEPENDS docs_interp docs_api
    COMMENT "Generating all documentation"
)

# Define the clean_docs target 
add_custom_target(clean_docs
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CCSD}/docs_interp
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CCSD}/docs_api
    COMMAND ${CMAKE_COMMAND} -E remove ${CCSD}/*_warn.log
    COMMENT "Cleaning documentation directories and log files"
)
