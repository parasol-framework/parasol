# Display module

set (MOD "display")

set (DISPLAY_SOURCES # Full paths are required for add_custom_command() dependencies to work correctly
   "${CMAKE_CURRENT_SOURCE_DIR}/display-driver.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_bitmap.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_display.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_clipboard.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_pointer.cpp")

set (DISPLAY_DEFS # Full paths are required for add_custom_command() dependencies to work correctly
   "${CMAKE_CURRENT_SOURCE_DIR}/class_bitmap_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_clipboard_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_display_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_pointer_def.c")

if (BUILD_DEFS)
   idl_def ("display.fdl" NAME ${MOD}_idl OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/idl.h")

   add_custom_command (OUTPUT
      ${DISPLAY_DEFS}
      "${DOCS}/modules/classes/bitmap.xml"
      "${DOCS}/modules/classes/clipboard.xml"
      "${DOCS}/modules/classes/display.xml"
      "${DOCS}/modules/classes/pointer.xml"
      PRE_BUILD
      COMMAND ${FLUID_CMD} ${IDL_C_SCRIPT} "--log-warning" "--src=${MOD}.fdl" "--sdk=${PROJECT_SOURCE_DIR}"
         "--files={" "${MOD}.cpp" "}"
      DEPENDS "${MOD}.fdl" ${DISPLAY_SOURCES}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Producing C definition files for the ${MOD} module"
      VERBATIM)

   #add_custom_target (display_defs DEPENDS ${DISPLAY_DEFS})
   #add_dependencies (build_headers display_defs)
endif ()

idl_c ("display.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   FILES "display-driver.cpp" "input_events.cpp"
   ARGS "--output-defs=module_def.c" "--output-proto=module_def.c" "--prototypes=static")

link_directories("/usr/X11R6/lib" "/usr/lib")

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

#target_sources (${MOD} PRIVATE "display.cpp")

target_include_directories (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/lib")

if (WIN32)
   target_link_libraries (${MOD} PRIVATE ole32 gdi32 uuid m)
   target_sources (${MOD} PRIVATE "display-driver.cpp" "win32/windows.c")
   add_definitions ("-DWIN_DRAGDROP")
elseif (ANDROID)
   # Android uses OpenGL.  A separate driver library is compiled for this purpose.
   add_library (display-gles1 SHARED)
   target_sources (display-gles1 PRIVATE "display-driver.cpp" "android/android.cpp")
   add_definitions ("-D_GLES_" "-D_GLES1_")
   target_link_libraries (display-gles1 PRIVATE m android EGL GLESv1_CM)
   # GL2 Link: -ljnigraphics -lEGL -lGLESv2 -DGLES2
   # GL3 Link: -ljnigraphics -lEGL -lGLESv3 -DGLES3
else ()
   # X11
   # Apt package requirements: libxrandr-dev libxxf86vm-dev libxxf86dga-dev
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-strict-overflow")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-overflow")
   target_sources (${MOD} PRIVATE "display-driver.cpp")
   target_link_libraries (${MOD} PRIVATE m Xext X11) # x11/libXxf86dga.a
   add_definitions ("-D__X11DGA__" "-D__xwindows__")
endif ()
