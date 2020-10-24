# Core Library

# IDL processing for headers and documentation

set (FUNCTIONS  # Full paths are required for add_custom_command() dependencies to work correctly
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_fields_read.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_fields_write.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_functions.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_messages.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_locking.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_newobject.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_events.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_actions.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_memory.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_log.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/core.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_keystore.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_base64.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_conversion.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_unicode.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_strings.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/lib_filesystem.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/fs_folders.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/fs_identify.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/fs_resolution.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/fs_volumes.c")

SET (CORE_DEFS
   "${CMAKE_CURRENT_SOURCE_DIR}/src/classes/class_compressed_stream_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/classes/class_config_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/classes/class_file_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/classes/class_script_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/classes/class_task_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/classes/class_thread_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/src/compression/class_compression_def.c")

if (BUILD_DEFS)
   add_custom_command (OUTPUT
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
      PRE_BUILD
      COMMAND ${FLUID_CMD} ${IDL_C_SCRIPT} "--log-error" "--src=errors.fdl" "--output=${INCLUDE_OUTPUT}/system/errors.h" "--sdk=${PROJECT_SOURCE_DIR}"
      COMMAND ${FLUID_CMD} ${IDL_C_SCRIPT} "--log-error" "--src=registry.fdl" "--output=${INCLUDE_OUTPUT}/system/registry.h" "--sdk=${PROJECT_SOURCE_DIR}"
      COMMAND ${FLUID_CMD} ${IDL_C_SCRIPT} "--log-error" "--src=fields.fdl" "--output=${INCLUDE_OUTPUT}/system/fields.h" "--sdk=${PROJECT_SOURCE_DIR}"
      COMMAND ${FLUID_CMD} ${IDL_C_SCRIPT} "--log-error" "--src=core.fdl" "--sdk=${PROJECT_SOURCE_DIR}" "--output-proto=src/prototypes.h" "--output-defs=src/data_functions.c" "--files={" ${FUNCTIONS} "}"
      COMMAND ${FLUID_CMD} ${IDL_DEF_SCRIPT} "--log-error" "--src=core.fdl" "--output=${CMAKE_CURRENT_SOURCE_DIR}/src/idl.h" "--sdk=${PROJECT_SOURCE_DIR}" "--format=c"
      DEPENDS "core.fdl" "errors.fdl" "registry.fdl" "fields.fdl" ${FUNCTIONS}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Producing definition files for the Core library"
      VERBATIM)

   add_custom_target (core_defs DEPENDS ${CORE_DEFS}
      "${INCLUDE_OUTPUT}/modules/core.h"
      "${INCLUDE_OUTPUT}/system/errors.h"
      "${INCLUDE_OUTPUT}/system/registry.h"
      "${INCLUDE_OUTPUT}/system/fields.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/idl.h")

   add_dependencies (build_headers core_defs)
endif ()

add_library (core SHARED)

set_module_defaults (core)

target_sources (core PRIVATE
   "src/core.c"
   "src/data.c"
   "src/data_actionlist.c"
   "src/lib_log.c"
   "src/lib_filesystem.c"
   "src/lib_functions.c"
   "src/lib_memory.c"
   "src/lib_newobject.c"
   "src/lib_actions.c"
   "src/lib_fields_read.c"
   "src/lib_fields_write.c"
   "src/lib_messages.c"
   "src/lib_locking.c"
   "src/internal.c"
   "src/lib_semaphores.c"
   "src/lib_events.c"
   "src/lib_keystore.c"
   "src/lib_strings.c"
   "src/classes/class_module.c"
   "src/classes/class_task.c"
   "src/classes/class_thread.c"
   "src/classes/class_time.c"
   "src/classes/class_config.c"
   "src/classes/class_metaclass.c"
   "src/classes/class_script.c"
   "src/classes/class_file.c"
   "src/classes/class_storagedevice.c"
   "src/classes/class_archive.c"
   "src/classes/class_compressed_stream.c"
   "src/compression/class_compression.c")

target_include_directories (core PRIVATE
   ${ZLIB_HEADERS})

# A bug in (MinGW?) GCC requires the following flag is applied to ensure that __packed__ works correctly.

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mno-ms-bitfields")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mno-ms-bitfields")
#target_compile_options (core PRIVATE "$<${GCC_COMPAT}:$<BUILD_INTERFACE:-mno-ms-bitfields>>")

# DBG_DIAGNOSTICS: Print enhanced diagnostic messages on crash - e.g. semaphore and memory locks.
# DBG_SLEEP: Debug any area of the system that puts the process to sleep.

add_definitions("-D__system__" "-DDBG_DIAGNOSTICS")

# These definitions tell the Core where the system files are installed.

if (RUN_ANYWHERE)
   add_definitions("-DROOT_PATH=" "-DSYSTEM_PATH=" "-DMODULE_PATH=\"parasol:lib/\"")
else ()
   add_definitions("-DROOT_PATH=\"${CMAKE_INSTALL_PREFIX}/\""
      "-DSYSTEM_PATH=\"${CMAKE_INSTALL_PREFIX}/share/parasol/\""
      "-DMODULE_PATH=\"${CMAKE_INSTALL_PREFIX}/lib/parasol/\"")
endif ()

target_link_libraries (core PRIVATE zlib)

if (WIN32)
   target_sources (core PRIVATE src/microsoft/windows.c)
   target_link_libraries (core PRIVATE wsock32 psapi)
   if (PARASOL_VLOG)
      # Header generation for libdbghelp: dlltool -k -d dbghelp.def -l libdbghelp.a
      # Copy libdbghelp.a to your MinGW libs folder.
      target_link_libraries (core PRIVATE dbghelp)
   endif ()
else ()
   target_link_libraries (core PRIVATE pthread dl)
endif ()

flute_test (core_filesystem "tests/filesystem.fluid")
flute_test (core_keypairs "tests/keypairs.fluid")
flute_test (core_misc "tests/misc.fluid")

# TODO: Convert these test builds to cmake
#$(PARASOL_RELEASE)/test_objlocking$(EXE): private_object_locking.c
#	$(CC) $(CFLAGS) $(EXELINK) $(CLIB) $(WINLINK) -o "$@" $< -lpthread
#
#$(PARASOL_RELEASE)/test_memlocking$(EXE): memory_locking.c
#	$(CC) $(CFLAGS) $(EXELINK) $(CLIB) $(WINLINK) -o "$@" $< -lpthread
