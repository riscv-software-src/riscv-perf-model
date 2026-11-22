add_library(execute
  Execute.cpp
  ExecutePipe.cpp
  IssueQueue.cpp
)
target_link_libraries(execute instgen)
