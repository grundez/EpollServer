add_executable (EpollServer 
"Server.cpp" "Server.h"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET EpollServer PROPERTY CXX_STANDARD 20)
endif()

