# Display module

set (MOD "display")
set (INC_MOD_DISPLAY TRUE PARENT_SCOPE)

set (FUNCTIONS # Full paths are required for add_custom_command() dependencies to work correctly
   "${CMAKE_CURRENT_SOURCE_DIR}/display-driver.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/lib_bitmap.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/lib_display.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/lib_cursor.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/lib_input.cpp")

set (CLASSES
   "${CMAKE_CURRENT_SOURCE_DIR}/class_bitmap.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_display.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_clipboard.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_controller.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_pointer.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_surface/class_surface.cpp")

set (DISPLAY_DEFS # Full paths are required for add_custom_command() dependencies to work correctly
   "${CMAKE_CURRENT_SOURCE_DIR}/class_bitmap_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_clipboard_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_controller_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_display_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_pointer_def.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/class_surface/surface_def.c")

if (BUILD_DEFS) # Customised idl_c() equivalent, this is for defining the output from the FDL file
   add_custom_command (OUTPUT
      ${DISPLAY_DEFS}
      "${DOCS}/modules/classes/bitmap.xml"
      "${DOCS}/modules/classes/clipboard.xml"
      "${DOCS}/modules/classes/controller.xml"
      "${DOCS}/modules/classes/display.xml"
      "${DOCS}/modules/classes/pointer.xml"
      "${DOCS}/modules/classes/surface.xml"
      COMMAND ${PARASOL_CMD} ${IDL_C_SCRIPT} "--log-warning" "src=${MOD}.fdl" "sdk=${PROJECT_SOURCE_DIR}"
              "output-proto=prototypes.h" "output-defs=module_def.c" "files={" ${FUNCTIONS} "lib_surfaces.cpp" "}"
      COMMAND ${PARASOL_CMD} ${IDL_COMPILE_SCRIPT} "--log-warning" "src=${MOD}.fdl" "output=${CMAKE_CURRENT_SOURCE_DIR}/idl.h"
              "sdk=${PROJECT_SOURCE_DIR}" "format=c"
      DEPENDS
         "${MOD}.fdl"
         ${FUNCTIONS}
         "lib_surfaces.cpp"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Producing C definition files for the ${MOD} module"
      VERBATIM)

   add_custom_target (display_defs DEPENDS ${DISPLAY_DEFS}
      "${INCLUDE_OUTPUT}/modules/display.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/idl.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/display.fdl")

   add_dependencies (build_headers display_defs)
endif ()

link_directories("/usr/X11R6/lib" "/usr/lib")

   CHECK_C_SOURCE_COMPILES ("
#include <X11/extensions/Xrandr.h>
int main() {
   return 0;
}"
   XRANDR_INSTALLED)

   CHECK_C_SOURCE_COMPILES ("
#include <X11/Xlib.h>
#include <X11/extensions/Xxf86dga.h>
int main() {
   return 0;
}"
   XDGA_INSTALLED)

if (XRANDR_INSTALLED)
   add_definitions ("-DXRANDR_ENABLED")
endif ()

if (XDGA_INSTALLED)
   add_definitions ("-DXDGA_ENABLED")
endif ()

add_library (${MOD})

set_module_defaults (${MOD} "Gfx")

target_include_directories (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/lib")

target_link_libraries (${MOD} PUBLIC unicode)
target_link_libraries (${MOD} PUBLIC linear_rgb)

if (WIN32)
   target_link_libraries (${MOD} PRIVATE ole32 gdi32 uuid xinput ${MATH_LINK})
   target_sources (${MOD} PRIVATE ${FUNCTIONS} "win32/windows.cpp" "win32/clipboard.c" ${CLASSES})
   add_compile_definitions ("WIN_DRAGDROP")
elseif (ANDROID)
   # Android uses OpenGL.  A separate driver library is compiled for this purpose.
   add_library (display-gles1 SHARED)
   target_sources (display-gles1 PRIVATE ${FUNCTIONS} "android/android.cpp" ${CLASSES})
   add_compile_definitions ("_GLES_" "_GLES1_")
   target_link_libraries (display-gles1 PRIVATE ${MATH_LINK} android EGL GLESv1_CM)
   # GL2 Link: -ljnigraphics -lEGL -lGLESv2 -DGLES2
   # GL3 Link: -ljnigraphics -lEGL -lGLESv3 -DGLES3
else ()
   # X11
   # Apt package requirements: libxrandr-dev libxxf86vm-dev libxxf86dga-dev
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-strict-overflow")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-overflow")
   target_sources (${MOD} PRIVATE ${FUNCTIONS} ${CLASSES})
   target_link_libraries (${MOD} PRIVATE ${MATH_LINK} Xrender Xrandr Xext X11) # x11/libXxf86dga.a
   add_compile_definitions ("__X11DGA__" "__xwindows__")
endif ()
