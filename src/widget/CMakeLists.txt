# Widget Module

set (MOD widget)

set (WIDGET_SOURCES # Full paths are required for add_custom_command() dependencies to work correctly
   "${CMAKE_CURRENT_SOURCE_DIR}/widget.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_resize.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_clipboard.cpp")

set (WIDGET_DEFS # Full paths are required for add_custom_command() dependencies to work correctly
   "${CMAKE_CURRENT_SOURCE_DIR}/class_clipboard_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_resize_def.c")

if (BUILD_DEFS)
   idl_def ("${MOD}.fdl" NAME widget_idl OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/${MOD}_def.c")

   idl_c ("hashes.fdl" NAME widget_hashes OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/hashes.h")

   add_custom_command (OUTPUT
      ${WIDGET_DEFS}
      "${DOCS}/modules/classes/clipboard.xml"
      "${DOCS}/modules/classes/resize.xml"
      PRE_BUILD
      COMMAND ${FLUID_CMD} ${IDL_C_SCRIPT} "--log-warning" "--src=${MOD}.fdl" "--sdk=${PROJECT_SOURCE_DIR}"
         "--files={" "${MOD}.cpp" "}"
      DEPENDS "${MOD}.fdl" ${WIDGET_SOURCES}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Producing C definition files for the ${MOD} module"
      VERBATIM)
   add_custom_target (widget_defs DEPENDS ${WIDGET_DEFS})
   add_dependencies (build_headers widget_defs)
endif ()

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

target_sources (${MOD} PRIVATE ${WIDGET_SOURCES})

if (WIN32)
   target_sources (${MOD} PRIVATE "platform/windows.c")
endif ()

if (WIN32)
   target_link_libraries (${MOD} PUBLIC ole32)
endif ()
