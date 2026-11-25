add_library(decode
  FusionDecode.cpp
  Decode.cpp
  MavisUnit.cpp
)
target_link_libraries(decode PUBLIC instruction FSL::api)
