add_library(vector
  VectorUopGenerator.cpp
)
target_link_libraries(vector PUBLIC core_inst core_cpu)
