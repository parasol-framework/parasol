
if (WIN32)
   add_library(init-win init-win.cpp)
   target_include_directories (init-win PRIVATE "${PROJECT_SOURCE_DIR}/core/include")
else ()
   add_library(init-unix init-unix.cpp)
   target_include_directories (init-unix PRIVATE "${PROJECT_SOURCE_DIR}/core/include")
   target_link_libraries (init-unix PRIVATE dl)
endif ()

