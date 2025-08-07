# Picture module
# There is a dependency on zlib for PNG support, but it's already linked from the Core.

set (MOD picture)
set (INC_MOD_PICTURE TRUE PARENT_SCOPE)

idl_gen ("${MOD}.fdl" NAME ${MOD}_defs APPEND_IDL "${MOD}_def.c" OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h")

add_subdirectory (libpng-1.6.50)

set (SOURCES "${MOD}.cpp")

add_library (${MOD})
target_include_directories (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/libpng-1.6.50")

set_module_defaults (${MOD} "Pic")
target_sources (${MOD} PRIVATE ${SOURCES})

target_link_libraries (${MOD} PUBLIC linear_rgb)
target_link_libraries (${MOD} PRIVATE ${MATH_LINK})

# Link with libpng - use static version if available, otherwise shared
if (TARGET png_static)
   target_link_libraries (${MOD} PRIVATE png_static)
elseif (TARGET png_shared)
   target_link_libraries (${MOD} PRIVATE png_shared)
endif()

