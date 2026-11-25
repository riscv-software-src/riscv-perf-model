add_library(vector
  VectorUopGenerator.cpp
)
target_link_libraries(vector PUBLIC instruction)
