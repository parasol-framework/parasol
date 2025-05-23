# Core Library

# IDL processing for headers and documentation

set (FUNCTIONS  # Full paths are required for add_custom_command() dependencies to work correctly
   "${CMAKE_CURRENT_SOURCE_DIR}/core.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/lib_objects.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/lib_events.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/lib_fields_read.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/lib_fields_write.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/lib_filesystem.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/lib_functions.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/lib_locking.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/lib_log.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/lib_messages.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/lib_memory.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/fs_folders.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/fs_identify.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/fs_resolution.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/fs_volumes.cpp")

SET (CORE_DEFS
   "${CMAKE_CURRENT_SOURCE_DIR}/compression/class_compressed_stream_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/classes/class_config_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/classes/class_file_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/classes/class_script_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/classes/class_task_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/classes/class_thread_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/compression/class_compression_def.c")

if (BUILD_DEFS)
   add_custom_command (
      OUTPUT
         ${CORE_DEFS}
         "${DOCS}/modules/classes/compressedstream.xml"
         "${DOCS}/modules/classes/compression.xml"
         "${DOCS}/modules/classes/config.xml"
         "${DOCS}/modules/classes/file.xml"
         "${DOCS}/modules/classes/metaclass.xml"
         "${DOCS}/modules/classes/module.xml"
         "${DOCS}/modules/classes/script.xml"
         "${DOCS}/modules/classes/storagedevice.xml"
         "${DOCS}/modules/classes/task.xml"
         "${DOCS}/modules/classes/thread.xml"
         "${DOCS}/modules/classes/time.xml"
         "${INCLUDE_OUTPUT}/modules/core.h"
         "${INCLUDE_OUTPUT}/system/errors.h"
         "${INCLUDE_OUTPUT}/system/registry.h"
         "${INCLUDE_OUTPUT}/system/fields.h"
         "${CMAKE_CURRENT_SOURCE_DIR}/idl.h"
      COMMAND ${PARASOL_CMD} ${IDL_C_SCRIPT} "--log-warning" "src=defs/errors.fdl" "output=${INCLUDE_OUTPUT}/system/errors.h" "sdk=${PROJECT_SOURCE_DIR}"
      COMMAND ${PARASOL_CMD} ${IDL_C_SCRIPT} "--log-warning" "src=defs/registry.fdl" "output=${INCLUDE_OUTPUT}/system/registry.h" "sdk=${PROJECT_SOURCE_DIR}"
      COMMAND ${PARASOL_CMD} ${IDL_C_SCRIPT} "--log-warning" "src=defs/fields.fdl" "output=${INCLUDE_OUTPUT}/system/fields.h" "sdk=${PROJECT_SOURCE_DIR}"
      COMMAND ${PARASOL_CMD} ${IDL_C_SCRIPT} "--log-warning" "src=defs/core.fdl" "sdk=${PROJECT_SOURCE_DIR}" "output-proto=prototypes.h" "output-defs=data_functions.c" "files={" ${FUNCTIONS} "}"
      COMMAND ${PARASOL_CMD} ${IDL_COMPILE_SCRIPT} "--log-warning" "src=defs/core.fdl" "output=${CMAKE_CURRENT_SOURCE_DIR}/idl.h" "sdk=${PROJECT_SOURCE_DIR}" "format=c"
      DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/defs/core.fdl"
         "${CMAKE_CURRENT_SOURCE_DIR}/defs/errors.fdl"
         "${CMAKE_CURRENT_SOURCE_DIR}/defs/registry.fdl"
         "${CMAKE_CURRENT_SOURCE_DIR}/defs/fields.fdl"
         "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/idl/common.fdl"
         ${FUNCTIONS}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Producing definition files for the Core library"
      VERBATIM)

   add_custom_target (core_defs DEPENDS ${CORE_DEFS}
      "${INCLUDE_OUTPUT}/modules/core.h"
      "${INCLUDE_OUTPUT}/system/errors.h"
      "${INCLUDE_OUTPUT}/system/registry.h"
      "${INCLUDE_OUTPUT}/system/fields.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/idl.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/defs/core.fdl"
      "${CMAKE_CURRENT_SOURCE_DIR}/defs/errors.fdl"
      "${CMAKE_CURRENT_SOURCE_DIR}/defs/registry.fdl"
      "${CMAKE_CURRENT_SOURCE_DIR}/defs/fields.fdl"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/idl/common.fdl")

   add_dependencies (build_headers core_defs)
endif ()

if (NOT MSVC)
   # A bug in (MinGW?) GCC requires the following flag is applied to ensure that __packed__ works correctly.
   set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mno-ms-bitfields")
   set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mno-ms-bitfields")
   #target_compile_options (core PRIVATE "$<${GCC_COMPAT}:$<BUILD_INTERFACE:-mno-ms-bitfields>>")
endif ()

# DBG_DIAGNOSTICS: Print enhanced diagnostic messages on crash - e.g. semaphore and memory locks.
# DBG_SLEEP: Debug any area of the system that puts the process to sleep.

add_compile_definitions ("__system__" "DBG_DIAGNOSTICS")

if (MSVC)
   add_compile_definitions ("_CRT_NONSTDC_NO_DEPRECATE")
endif ()

# These definitions tell the Core where the system files are installed.

if (RUN_ANYWHERE)
   add_compile_definitions("_ROOT_PATH=" "_SYSTEM_PATH=" "_MODULE_PATH=\"parasol:lib/\"")
else ()
   add_compile_definitions ("_ROOT_PATH=\"${CMAKE_INSTALL_PREFIX}/\""
      "_SYSTEM_PATH=\"${CMAKE_INSTALL_PREFIX}/share/parasol/\""
      "_MODULE_PATH=\"${CMAKE_INSTALL_PREFIX}/lib/parasol/\"")
endif ()

add_library (core)

set_module_defaults (core)

target_sources (core PRIVATE
   "core.cpp"
   "data.cpp"
   "data_actionlist.cpp"
   "internal.cpp"
   "lib_objects.cpp"
   "lib_events.cpp"
   "lib_log.cpp"
   "lib_fields_read.cpp"
   "lib_fields_write.cpp"
   "lib_filesystem.cpp"
   "lib_functions.cpp"
   "lib_locking.cpp"
   "lib_memory.cpp"
   "lib_messages.cpp"
   "classes/class_config.cpp"
   "classes/class_file.cpp"
   "classes/class_metaclass.cpp"
   "classes/class_module.cpp"
   "classes/class_script.cpp"
   "classes/class_storagedevice.cpp"
   "classes/class_task.cpp"
   "classes/class_thread.cpp"
   "classes/class_time.cpp"
   "compression/class_compression.cpp")

target_include_directories (core PRIVATE
   ${ZLIB_HEADERS})

target_link_libraries (core PRIVATE zlib)

if (PARASOL_STATIC)
   target_link_libraries (core PRIVATE ${LIB_LIST})
endif ()

if (WIN32)
   target_sources (core PRIVATE microsoft/windows.cpp)
   target_link_libraries (core PRIVATE wsock32 psapi)
   if (PARASOL_VLOG OR MSVC)
      # Header generation for libdbghelp: dlltool -k -d dbghelp.def -l libdbghelp.a
      # Copy libdbghelp.a to your MinGW libs folder.
      target_link_libraries (core PRIVATE dbghelp)
   endif ()
else ()
   target_link_libraries (core PRIVATE pthread dl)
endif ()

flute_test (core_filesystem "tests/filesystem.fluid")
flute_test (core_misc "tests/misc.fluid")
flute_test (core_compression "tests/compression.fluid")
flute_test (core_config "tests/config.fluid")

if (INSTALL_TESTS)
   add_executable (test_objlocking "tests/object_locking.cpp")
   add_executable (test_memlocking "tests/memory_locking.cpp")

   target_link_libraries (test_objlocking PRIVATE ${INIT_LINK})
   target_link_libraries (test_memlocking PRIVATE ${INIT_LINK})

   set_target_properties (test_memlocking test_objlocking PROPERTIES CXX_STANDARD 20 LINK_FLAGS_RELEASE "-s")

   install (TARGETS test_objlocking test_memlocking DESTINATION "tests/")
endif ()
