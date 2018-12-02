# SETTINGS
# --------
# Environment variables that can be set from your command line:
#
#   CC:
#      The C compiler (defaults to gcc)
#   CXX
#      The C++ compiler (defaults to g++)
#   PARASOL_RELEASE:
#      Refers to the location of a core installation that contains the 'system' folder.
#   OSTYPE:
#      Set to 'win', 'linux' or 'android'
#      When compiling for Android, be sure to use the 'android' setting and not your machine's OS.
#   CPU:
#      Either 'x86' or 'x64'
#   NDK:
#      For Android only.  On Linux this is normally set to '/ndk'.  On Windows, use conventional paths, e.g. 'D:\ndk'.
#   VDEBUG=1
#      Optional.  Turns on all debugging messages in the compiled binaries (results in extremely verbose debug logs).
#   DEBUG=1
#      Optional.  For developers that need to debug using gdb.
#
# Compile problems may occur when using different versions of GCC.  For example GCC 4.x requires use of
# -Wno-pointer-sign, earlier versions do not.

lc = $(subst A,a,$(subst B,b,$(subst C,c,$(subst D,d,$(subst E,e,$(subst F,f,$(subst G,g,$(subst H,h,$(subst I,i,$(subst J,j,$(subst K,k,$(subst L,l,$(subst M,m,$(subst N,n,$(subst O,o,$(subst P,p,$(subst Q,q,$(subst R,r,$(subst S,s,$(subst T,t,$(subst U,u,$(subst V,v,$(subst W,w,$(subst X,x,$(subst Y,y,$(subst Z,z,$1))))))))))))))))))))))))))

ifndef SDK_ROOT
   this := $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))
   ifeq (/,$(findstring /,$(this)))
      SDK_ROOT := $(shell x=$(this) && echo $${x%/*})
      SDK_ROOT := $(shell cd $(SDK_ROOT) && pwd)
   endif

   ifndef SDK_ROOT
      SDK_ROOT := `pwd`
   endif
endif

.DEFAULT_GOAL ?= compile

UNAME ?= $(shell uname -s)
OSTYPE ?= $(UNAME)
OSTYPE := $(call lc,$(OSTYPE))
UNAME := $(call lc,$(UNAME))
XXD ?= $(shell command -v xxd 2> /dev/null)

MSYSTEM_PREFIX ?= /mingw
WINDRES ?= windres

# OSTYPE indicates the underlying OS kernel.
# HOST indicates the OS architecture encapsulating the kernel.

ifeq ($(MSYSTEM),MINGW32) # Matches MSYS2 running mingw32.exe
   HOST   := win
   OSTYPE := win
   CPU    ?= x86
else ifeq ($(MSYSTEM),MSYS) # Matches MSYS2 running mingw64.exe
   HOST   := win
   OSTYPE := win
   CPU    ?= x86
else ifeq ($(OSTYPE),cygwin)
   HOST   := win
   OSTYPE := win
   CPU    ?= x86
else ifeq ($(OSTYPE),darwin)
   HOST   := mac
   OSTYPE := unix
   CPU    ?= $(shell uname -m)
else
   HOST   := unix
   OSTYPE := unix
   CPU    ?= $(shell uname -m)
endif

ifeq ($(CPU),x86_64)
   CPUBITS    = 64
   ARCHEXT    = -x64
   ARCH       = -m$(CPUBITS)
   ANDROIDCPU = x86_64
   OBJARCH    = i386
   OBJOUTPUT  = pei-x86-64
else
   CPUBITS    = 32
   ARCH       = -march=pentium4  -m$(CPUBITS)
   ANDROIDCPU = x86
   OBJARCH    = i386
   OBJOUTPUT  = a.out-i386-linux
endif

PLATFORM = $(HOST)_$(CPU)
LIBNAME  = $(NAME)

PARASOL_RELEASE ?= $(SDK_ROOT)/release/$(call lc,$(PLATFORM))
PARASOL_INSTALL ?= $(SDK_ROOT)/install

# Use the installed parasol executable if available; fallback to the release folder; otherwise do nothing.

PARASOL_CMD_EXISTS ?= $(shell which parasol 2> /dev/null)

ifeq ($(strip $(PARASOL_CMD_EXISTS)),)
   PARASOL_CMD_VERIFY := $(shell $(PARASOL_RELEASE)/parasol --log-none --verify)
   ifeq ($(strip $(PARASOL_CMD_VERIFY)),1)
      PARASOL_CMD := $(PARASOL_RELEASE)/parasol
      FLUID_CMD := $(PARASOL_RELEASE)/fluid
   else
      # If parasol is unreliable, don't allow it to run.
      PARASOL_CMD := : parasol
      FLUID_CMD := : fluid
   endif
else
   PARASOL_CMD := parasol
   FLUID_CMD := fluid
endif

FLUTE = $(FLUID_CMD) --log-error "$(SDK_ROOT)/scripts/flute.fluid"
IDL_C = $(FLUID_CMD) --log-error "$(SDK_ROOT)/scripts/idl/idl-c.fluid"
IDL_DEF = $(FLUID_CMD) --log-error "$(SDK_ROOT)/scripts/idl/idl-def.fluid"

ifeq ($(OSTYPE),android)
   # Android settings

   ifndef ANDROIDHOST
      ifeq ($(TERM),cygwin)
         ANDROIDHOST=windows
      else
         ifeq ($(OS),Windows_NT)
            ANDROIDHOST=windows
         else
            ANDROIDHOST=linux
         endif
      endif
   endif

   NDK ?= /ndk

   ifeq ($(ANDROIDHOST),windows)
      NDK := $(shell cygpath -m $(NDK))
   endif

   CPU:= arm
   CPUBITS:= 32
   ARCH=
   TOOLVER:=4.8
   # Valid values: armeabi, armeabi-v7a, x86
   CXX_TARGET ?= armeabi

   # Refer to "$(NDK)/platforms/android-" for valid targets.  This will affect the minimum supported version of Android.
   ANDROID_TARGET=9
   ANDROID_HOME=/cygdrive/c/Program\ Files\ \(x86\)/Android/android-sdk
   TOOLCHAIN= $(NDK)/toolchains/arm-linux-androideabi-$(TOOLVER)/prebuilt/$(ANDROIDHOST)-$(ANDROIDCPU)/bin/arm-linux-androideabi-
   CC=    $(TOOLCHAIN)gcc --sysroot "$(SYSROOT)"
   CXX=   $(TOOLCHAIN)g++ --sysroot "$(SYSROOT)"
   GDB=   $(TOOLCHAIN)gdb
   AAPT=  $(ANDROID_HOME)/platforms/android-3/tools/aapt
   ADB=   $(ANDROID_HOME)/platform-tools/adb
   ADX=   $(ANDROID_HOME)/build-tools/*/dx.bat
   ASM=   $(TOOLCHAIN)as
   ZIPALIGN= $(ANDROID_HOME)/tools/zipalign
   OBJCOPY=  $(TOOLCHAIN)objcopy
   SYSROOT=  $(NDK)/platforms/android-$(ANDROID_TARGET)/arch-$(CPU)
   NDKGDB=   $(NDK)/ndk-gdb

   LIBNAME= lib_$(NAME)
   PARASOL_MODULES= $(SDK_ROOT)/jni/libs/armeabi
   PARASOL_MODULE=  $(PARASOL_MODULES)/$(LIBNAME)$(LIBEXT)
   PARASOL_ROOTMOD= $(PARASOL_MODULES)/lib$(NAME)$(LIBEXT)

   CFLAGS= -B $(NDK)/platforms/android-$(ANDROID_TARGET)/arch-$(CPU)/usr/include
   CXXFLAGS= -I$(NDK)/sources/cxx-stl/gnu-libstdc++/$(TOOLVER)/include -I$(NDK)/sources/cxx-stl/gnu-libstdc++/$(TOOLVER)/libs/$(CXX_TARGET)/include
   OBJFLAGS= -B arm -O elf32-littlearm
   LIBLINK=  -shared -nostartfiles -Wl,-soname,"$(LIBNAME)$(LIBEXT)",-Bsymbolic -o "$(DEST)"
   LIBLINK_STDC=  -shared -Wl,-soname,"$(LIBNAME)$(LIBEXT)",-Bsymbolic -o "$(DEST)"
   LIBLINK_STDCPP= -L$(NDK)/sources/cxx-stl/gnu-libstdc++/$(TOOLVER)/libs/$(CXX_TARGET) -lgnustl_static -lsupc++ -shared -Wl,-soname,"$(LIBNAME)$(LIBEXT)",-Bsymbolic -o "$(DEST)"
   LIBEXT=   .so
   EXE=
   EXELINK_INIT= "$(PARASOL_LIB_INIT)"

else
   ifeq ($(OSTYPE),win)
      # Microsoft Windows settings

      PRESYM=
      CXX ?= g++
      ifeq ($(CC),cc)
         CC := gcc
      else
         CC ?= gcc
      endif
      ifeq ($(DEBUG),1)
         ASM= nasm -g -f win$(CPUBITS) -d M_TARGET_WIN32 --prefix _
      else
         ASM= nasm -O2 -f win$(CPUBITS) -d M_TARGET_WIN32 --prefix _
      endif
      CFLAGS=  -std=gnu11 -fdata-sections -ffunction-sections $(ARCH) -D_WIN32
      OBJFLAGS= -B $(OBJARCH) -O pe-i386 --prefix-symbols _
      # PARASOL_DLL refers to init-dll.c, which contains dummy function DllMainCRTStartup() for the purpose of DLL creation.
      LIBLINK= -Wl,--gc-sections -shared -nostartfiles -o "$(DEST)" "$(PARASOL_DLL)"
      LIBLINK_STDC= -Wl,--gc-sections -shared -o "$(DEST)"
      LIBLINK_STDCPP= -Wl,--gc-sections -shared -o "$(DEST)"
      LIBDEP=      "$(PARASOL_DLL)"
      LIBEXT= .dll
      EXE=    .exe
      EXEFLAGS= -Wall -mwindows
      EXELINK_INIT= -Wall -mwindows "$(PARASOL_LIB_INIT)" "$(PARASOL_RES)"
   else
      # Linux & Unix settings

      CXX ?= g++
      CC ?=  gcc
      CFLAGS=   -fPIC  -fdata-sections -ffunction-sections
      CXXFLAGS= -fPIC  -fdata-sections -ffunction-sections
      OBJFLAGS= -B $(OBJARCH) --output $(OBJOUTPUT)
      ASM=      nasm -f elf$(CPUBITS) -d M_TARGET_LINUX
      LIBLINK=         -shared -nostartfiles -Wl,-soname,"$(DEST)",-Bsymbolic -o "$(DEST)"
      LIBLINK_STDC=    -shared -Wl,-soname,"$(DEST)",-Bsymbolic -o "$(DEST)"
      LIBLINK_STDCPP=  -shared -Wl,-soname,"$(DEST)",-Bsymbolic -o "$(DEST)"
      LIBEXT=   .so
      EXE=
      EXELINK_INIT= "$(PARASOL_LIB_INIT)" -ldl

      # OSX specific overrides

      ifeq ($(HOST),mac)
         LIBLINK=         -shared -nostartfiles -o "$(DEST)"
         LIBLINK_STDC=    -shared -o "$(DEST)"
         LIBLINK_STDCPP=  -shared -o "$(DEST)"
      endif
   endif
endif

EXEDEP_INIT= "$(PARASOL_LIB_INIT)"

# NB: $(ARCHEXT) is use to compartmentalise compilations for different CPU's.
PARASOL_MODULES ?= $(PARASOL_RELEASE)/system/modules$(ARCHEXT)

#PARASOL_MODULE ?= $(PARASOL_MODULES)/$(if $(CATEGORY),$(CATEGORY)_,)$(LIBNAME)$(LIBEXT)
PARASOL_MODULE  ?= $(PARASOL_MODULES)/$(LIBNAME)$(LIBEXT)
PARASOL_ROOTMOD ?= $(PARASOL_MODULE)
MODULE_DEFS     ?= $(PARASOL_RELEASE)/system/config/defs

# PARASOL_HEADERS - include path is based on the location of our makefile.inc file.

ifndef PARASOL_HEADERS
   PARASOL_HEADERS := $(if $(SDK_ROOT),$(SDK_ROOT)/core/,)include
endif
#-Wno-narrowing
CFLAGS+= -Wall $(ARCH) -fno-strict-aliasing -Wno-empty-body -Wno-unused-result -Wno-pointer-sign -Wno-format-zero-length -I$(PARASOL_HEADERS) -I$(SDK_ROOT)/core/link -I$(SDK_ROOT)/core/3rdparty/symlib
CXXFLAGS+= -Wall $(ARCH) -fno-strict-aliasing -Wno-empty-body -Wno-unused-result -Wno-missing-braces -Wno-char-subscripts -I$(PARASOL_HEADERS) -I$(SDK_ROOT)/core/link -I$(SDK_ROOT)/core/3rdparty/symlib

ifneq ($(HOST),mac)
   CFLAGS+= -Wno-unused-but-set-variable
   CXXFLAGS+= -Wno-unused-but-set-variable
endif

# VDEBUG: Verbose debugging
# DEBUG: Symbolic debugging (gdb)

ifeq ($(VDEBUG),1)
   CFLAGS+= -g -DDEBUG
   CXXFLAGS+= -g -DDEBUG
   DEBUG ?= 1
else
   ifeq ($(DEBUG),1)
      CFLAGS+= -g
      CXXFLAGS+= -g
   else
      CFLAGS   += -O2
      CXXFLAGS += -O2
      ifneq ($(HOST),mac)
         LINK      = -s
         LIBLINK  += -s
         EXELINK  += -s
      endif
   endif
endif

# CFLAGS += -fno-tree-slp-vectorize -fno-tree-loop-vectorize \
# -fno-inline-functions  -fno-vect-cost-model -fno-tree-partial-pre -fno-tree-loop-distribute-patterns \
# -fno-unswitch-loops -fno-predictive-commoning -fno-gcse-after-reload \
# -fno-peel-loops -fno-ipa-cp-clone

OBJCOPY ?= objcopy

C11FLAGS= $(CXXFLAGS) -std=c++0x -Wno-deprecated-declarations

ifeq ($(OSTYPE),win)
   PARASOL_LIB_INIT := $(SDK_ROOT)/core/link/init-win$(ARCHEXT).o
   PARASOL_DLL := $(SDK_ROOT)/core/link/init-dll$(ARCHEXT).o
   PARASOL_RES := $(SDK_ROOT)/core/link/resource$(ARCHEXT).o
else
   PARASOL_LIB_INIT := $(SDK_ROOT)/core/link/init-unix$(ARCHEXT).o
endif

# These environment variables help GCC find the Parasol headers

ifeq ($(OSTYPE),win)
C_INCLUDE_PATH :=     $(C_INCLUDE_PATH);$(PARASOL_HEADERS);$(SDK_ROOT)/core/3rdparty/symlib;$(SDK_ROOT)/core/link
CPLUS_INCLUDE_PATH := $(CPLUS_INCLUDE_PATH);$(PARASOL_HEADERS);$(SDK_ROOT)/core/3rdparty/symlib;$(SDK_ROOT)/core/link
else
C_INCLUDE_PATH :=     $(C_INCLUDE_PATH):$(PARASOL_HEADERS):$(SDK_ROOT)/core/3rdparty/symlib:$(SDK_ROOT)/core/link
CPLUS_INCLUDE_PATH := $(CPLUS_INCLUDE_PATH):$(PARASOL_HEADERS):$(SDK_ROOT)/core/3rdparty/symlib:$(SDK_ROOT)/core/link
endif

.PHONY: compile
default: compile

ifeq ($(OSTYPE),win)
"$(PARASOL_DLL)": $(SDK_ROOT)/core/link/init-dll.c
	@make -C "$(SDK_ROOT)/core/link"

"$(PARASOL_RES)": $(SDK_ROOT)/core/link/init-$(OSTYPE).c
	@make -C "$(SDK_ROOT)/core/link"
endif

"$(PARASOL_LIB_INIT)": $(SDK_ROOT)/core/link/init-$(OSTYPE).c
	@make -C "$(SDK_ROOT)/core/link"
