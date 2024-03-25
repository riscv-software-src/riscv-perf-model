project(BranchPred_test)

add_executable(BranchPred_test BranchPred_test.cpp)
target_link_libraries(BranchPred_test core common_test SPARTA::sparta)

sparta_named_test(BranchPred_test_Run  BranchPred_test)
