add_library(execute
  Execute.cpp
  ExecutePipe.cpp
  IssueQueue.cpp
)
target_link_libraries(execute PUBLIC core_inst core_cpu)
