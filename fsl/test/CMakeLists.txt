cmake_minimum_required(VERSION 3.10)
project(Test)

# Include subdirectories
add_subdirectory(api)
add_subdirectory(interp)

# Add test targets
add_custom_target(run_test_interp
    COMMAND ${CMAKE_COMMAND} 
       --build ${CMAKE_CURRENT_BINARY_DIR}/interp --target run_interp_tests
    COMMENT "Running interp tests"
)

add_custom_target(run_test_api
    COMMAND ${CMAKE_COMMAND} 
       --build ${CMAKE_CURRENT_BINARY_DIR}/api --target run_api_tests
    COMMENT "Running API tests"
)

# Add the regress target to run all tests
# Customize the name to avoid conflict with Oly's regress
add_custom_target(fsl_regress ALL
    DEPENDS run_test_interp run_test_api
    COMMENT "Running all tests"
)

