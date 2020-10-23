# Picture module

set (MOD picture)

idl_all ("${MOD}.fdl" NAME ${MOD}_defs APPEND_IDL "${MOD}_def.c" OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h")

set (SOURCES "${MOD}.c"
   "lib/png.c" "lib/pngset.c" "lib/pngget.c" "lib/pngread.c" "lib/pngpread.c"
   "lib/pngrtran.c" "lib/pngrutil.c" "lib/pngtrans.c" "lib/pngwrite.c" "lib/pngwtran.c"
   "lib/pngwutil.c" "lib/pngmem.c" "lib/pngerror.c")

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

target_sources (${MOD} PRIVATE ${SOURCES})

target_include_directories (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/lib"
   ${ZLIB_HEADERS})

target_link_libraries (${MOD} PRIVATE m) # zlib does not need to be linked as this is done by the core

