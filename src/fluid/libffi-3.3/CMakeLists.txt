
# The scripts below will produce SOURCES_LIST
include (configure_platform.cmake)
include (configure_options.cmake)

file (COPY ${FFI_CONFIG_FILE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file (COPY src/${TARGETDIR}/ffitarget.h DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include)

include_directories (${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories (${CMAKE_CURRENT_BINARY_DIR})
include_directories (include)

add_library (libffi STATIC)
set_property (TARGET libffi PROPERTY POSITION_INDEPENDENT_CODE 1)

target_sources (libffi PRIVATE
   src/closures.c src/java_raw_api.c src/prep_cif.c src/raw_api.c src/types.c
   ${SOURCES_LIST})

if (MSVC)
   #set (CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif ()
