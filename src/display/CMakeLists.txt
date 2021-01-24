# Display module

set (MOD "display")

idl_def ("display.fdl" NAME ${MOD}_idl OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/idl.h")

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
