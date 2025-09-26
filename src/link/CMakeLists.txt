
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED True)
set (CMAKE_POSITION_INDEPENDENT_CODE ON) # Enables -fPIC and -fPIE in gcc

if (WIN32)
   add_library(init-win STATIC init-win.cpp)
   target_include_directories (init-win PRIVATE "${PROJECT_SOURCE_DIR}/core/include")
else ()
   add_library(init-unix STATIC init-unix.cpp)
   target_include_directories (init-unix PRIVATE "${PROJECT_SOURCE_DIR}/core/include")
   target_link_libraries (init-unix PRIVATE dl)
endif ()

add_library(link_regex OBJECT regex.cpp)
target_include_directories (link_regex PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

