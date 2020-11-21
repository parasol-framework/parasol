# Widget Module

set (MOD widget)

set (WIDGET_SOURCES # Full paths are required for add_custom_command() dependencies to work correctly
   "${CMAKE_CURRENT_SOURCE_DIR}/widget.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_button.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_checkbox.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_resize.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_scrollbar.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_combobox.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_tabfocus.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_input.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_scroll.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_image.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_text/text.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_menu/menu.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_view/view.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_clipboard.c")

set (WIDGET_DEFS # Full paths are required for add_custom_command() dependencies to work correctly
   "${CMAKE_CURRENT_SOURCE_DIR}/class_button_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_checkbox_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_clipboard_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_combobox_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_image_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_input_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_resize_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_scroll_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_scrollbar_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_tabfocus_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_menu/menu_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_text/def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_view/view_def.c")

if (BUILD_DEFS)
   idl_def ("${MOD}.fdl" NAME widget_idl OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/${MOD}_def.c")

   idl_c ("hashes.fdl" NAME widget_hashes OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/hashes.h")

   add_custom_command (OUTPUT
      ${WIDGET_DEFS}
      "${DOCS}/modules/classes/input.xml"
      "${DOCS}/modules/classes/clipboard.xml"
      "${DOCS}/modules/classes/view.xml"
      "${DOCS}/modules/classes/button.xml"
      "${DOCS}/modules/classes/menuitem.xml"
      "${DOCS}/modules/classes/scroll.xml"
      "${DOCS}/modules/classes/combobox.xml"
      "${DOCS}/modules/classes/menu.xml"
      "${DOCS}/modules/classes/resize.xml"
      "${DOCS}/modules/classes/checkbox.xml"
      "${DOCS}/modules/classes/text.xml"
      "${DOCS}/modules/classes/scrollbar.xml"
      "${DOCS}/modules/classes/image.xml"
      "${DOCS}/modules/classes/tabfocus.xml"
      PRE_BUILD
      COMMAND ${FLUID_CMD} ${IDL_C_SCRIPT} "--log-error" "--src=${MOD}.fdl" "--sdk=${PROJECT_SOURCE_DIR}"
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
