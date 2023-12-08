project(dump_inst)

add_executable       (dump_inst dump_inst.cpp)
include_directories  (${SIM_BASE})
target_link_libraries(dump_inst core mavis SPARTA::sparta)
