# JPEG module

set (MOD jpeg)

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

target_sources (${MOD} PRIVATE
   "jpeg.cpp"
   "lib/jfdctfst.c"
   "lib/jfdctflt.c"
   "lib/jfdctint.c"
   "lib/jcapimin.c"
   "lib/jcapistd.c"
   "lib/jccoefct.c"
   "lib/jcsample.c"
   "lib/jccolor.c"
   "lib/jcdctmgr.c"
   "lib/jchuff.c"
   "lib/jcinit.c"
   "lib/jcmainct.c"
   "lib/jcmarker.c"
   "lib/jcmaster.c"
   "lib/jcprepct.c"
   "lib/jcomapi.c"
   "lib/jcparam.c"
   "lib/jcphuff.c"
   "lib/jctrans.c"
   "lib/jdapimin.c"
   "lib/jdapistd.c"
   "lib/jdatadst.c"
   "lib/jdatasrc.c"
   "lib/jdcoefct.c"
   "lib/jdcolor.c"
   "lib/jddctmgr.c"
   "lib/jdhuff.c"
   "lib/jdinput.c"
   "lib/jdmainct.c"
   "lib/jdmarker.c"
   "lib/jdmaster.c"
   "lib/jdmerge.c"
   "lib/jdphuff.c"
   "lib/jdpostct.c"
   "lib/jdsample.c"
   "lib/jdtrans.c"
   "lib/jerror.c"
   "lib/jidctflt.c"
   "lib/jidctfst.c"
   "lib/jidctint.c"
   "lib/jidctred.c"
   "lib/jmemansi.c"
   "lib/jmemmgr.c"
   "lib/jquant1.c"
   "lib/jquant2.c"
   "lib/jutils.c")

target_include_directories (${MOD} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lib")

target_link_libraries (${MOD} PRIVATE "m")
