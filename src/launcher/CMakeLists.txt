# Parasol executable

# Generate version information based on current date (YYYY.MM.DD format)
string(TIMESTAMP PARASOL_VERSION_MAJOR "%Y")
string(TIMESTAMP PARASOL_VERSION_MINOR "%m")
string(TIMESTAMP PARASOL_VERSION_PATCH "%d")
string(TIMESTAMP PARASOL_COPYRIGHT_YEAR "%Y")

# Remove leading zeros from month and day for proper version format
string(REGEX REPLACE "^0+" "" PARASOL_VERSION_MINOR "${PARASOL_VERSION_MINOR}")
string(REGEX REPLACE "^0+" "" PARASOL_VERSION_PATCH "${PARASOL_VERSION_PATCH}")

# Ensure we don't get empty strings for single digit values
if(PARASOL_VERSION_MINOR STREQUAL "")
    set(PARASOL_VERSION_MINOR "0")
endif()
if(PARASOL_VERSION_PATCH STREQUAL "")
    set(PARASOL_VERSION_PATCH "0")
endif()

message(STATUS "Parasol version: ${PARASOL_VERSION_MAJOR}.${PARASOL_VERSION_MINOR}.${PARASOL_VERSION_PATCH}")

# Generate versioned manifest and resource files
configure_file(
    "${PROJECT_SOURCE_DIR}/src/link/parasol.manifest.in"
    "${PROJECT_SOURCE_DIR}/src/link/parasol.manifest"
    @ONLY
)

configure_file(
    "${PROJECT_SOURCE_DIR}/src/link/resource.rc.in"
    "${PROJECT_SOURCE_DIR}/src/link/resource.rc"
    @ONLY
)

if (RUN_ANYWHERE)
   set (CMAKE_INSTALL_RPATH "$ORIGIN/lib/lib")
else ()
   set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/parasol")
endif ()

if (WIN32)
   # Specifying the WIN32 option will generate a Windows GUI application
   add_executable (parasol_cmd WIN32 "parasol.cpp" "sandbox-win.cpp" "${PROJECT_SOURCE_DIR}/src/link/resource.rc")

   # Disable automatic manifest generation since we provide our own
   if (MSVC)
      set_target_properties(parasol_cmd PROPERTIES LINK_FLAGS "/MANIFEST:NO")
   endif()
else ()
   add_executable (parasol_cmd "parasol.cpp")
endif ()

target_link_libraries (parasol_cmd PRIVATE ${INIT_LINK})

target_include_directories (parasol_cmd PRIVATE "${PROJECT_SOURCE_DIR}/src/link")

if (CMAKE_BUILD_TYPE MATCHES "Release")
   message (STATUS "Symbols will be stripped from this build (no debugging).")
   set_target_properties (parasol_cmd PROPERTIES CXX_STANDARD 20 LINK_FLAGS_RELEASE "-s" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release")
else ()
   message (STATUS "Symbols will be left intact in this build (debug enabled).")
   set_target_properties (parasol_cmd PROPERTIES CXX_STANDARD 20 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release")
endif ()

set_target_properties (parasol_cmd PROPERTIES OUTPUT_NAME "parasol")

# Attach headers to the Parasol binary so that we can ensure that all headers are built

add_dependencies (parasol_cmd build_headers)
add_dependencies (parasol_cmd ${REQUIRED_MODS})

if (PARASOL_INSTALL)
   if (WIN32 AND NOT MSVC)
      # MSYS2/MinGW builds have library dependencies that we need to take a copy of.
      find_file(MSYS_DEP2 "libstdc++-6.dll" HINTS "/mingw64/bin")
      if (NOT MSYS_DEP2 STREQUAL "MSYS_DEP2-NOTFOUND")
         install (FILES ${MSYS_DEP2} DESTINATION .)
      endif ()

      find_file(MSYS_DEP4 "libgcc_s_seh-1.dll" HINTS "/mingw64/bin")
      if (NOT MSYS_DEP4 STREQUAL "MSYS_DEP4-NOTFOUND")
         install (FILES ${MSYS_DEP4} DESTINATION .)
      endif ()

      find_file(MSYS_DEP5 "libwinpthread-1.dll" HINTS "/mingw64/bin")
      if (NOT MSYS_DEP5 STREQUAL "MSYS_DEP5-NOTFOUND")
         install (FILES ${MSYS_DEP5} DESTINATION .)
      endif ()
   endif ()

   if (RUN_ANYWHERE)
      install (TARGETS parasol_cmd DESTINATION .)
   else ()
      install (TARGETS parasol_cmd DESTINATION "bin")
   endif ()
endif ()
