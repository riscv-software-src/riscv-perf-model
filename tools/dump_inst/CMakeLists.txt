project(dump_inst)

add_executable       (dump_inst dump_inst.cpp)
include_directories  (${SIM_BASE})
target_link_libraries(dump_inst core mavis SPARTA::sparta)

file(CREATE_LINK ${SIM_BASE}/arches     ${CMAKE_CURRENT_BINARY_DIR}/arches          SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
