@rem Script to build LuaJIT with MSVC.
@rem Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
@rem
@rem Open a "Visual Studio Command Prompt" (either x86 or x64).
@rem Then cd to this directory and run this script. Use the following
@rem options (in order), if needed. The default is a dynamic release build.
@rem
@rem   nogc64   disable LJ_GC64 mode for x64
@rem   debug    emit debug symbols

@if not defined INCLUDE goto :FAIL

@rem Check for custom linker before @setlocal
@rem Find MSVC linker in the same directory as cl.exe to avoid Git's link.exe
@if not defined LJLINK (
   for %%i in (cl.exe) do set "LJLINK=%%~dp$PATH:ilink.exe"
)
@if not defined LJLINK_ARGS set "LJLINK_ARGS=/nologo"

@setlocal
@rem Add more debug flags here, e.g. DEBUGCFLAGS=/DLUA_USE_APICHECK
@set DEBUGCFLAGS=
@set LJCOMPILE=cl /nologo /c /O2 /W3 /DLUAJIT_ENABLE_LUA52COMPAT /D_CRT_SECURE_NO_DEPRECATE /D_CRT_STDIO_INLINE=__declspec(dllexport)__inline
@set LJMT=mt /nologo
@set LJLIB=lib /nologo /nodefaultlib
@set DASMDIR=..\dynasm
@set DASM=%DASMDIR%\dynasm.lua
@set DASC=vm_x64.dasc
@set LJLIBNAME=lua51.lib
@set BUILDTYPE=release
@set ALL_LIB=lib_base.cpp lib_math.cpp lib_bit.cpp lib_string.cpp lib_table.cpp lib_io.cpp lib_os.cpp lib_package.cpp lib_debug.cpp lib_jit.cpp lib_ffi.cpp lib_buffer.cpp

@rem Incremental builds are enabled. Comment out the line below to disable.
@rem if exist %LJLIBNAME% exit 0

%LJCOMPILE% host\minilua.cpp
@if errorlevel 1 goto :BAD
"%LJLINK%" %LJLINK_ARGS% /out:minilua.exe minilua.obj
@if errorlevel 1 goto :BAD
if exist minilua.exe.manifest^
  %LJMT% -manifest minilua.exe.manifest -outputresource:minilua.exe

@set DASMFLAGS=-D WIN -D JIT -D FFI -D P64
@set LJARCH=x64
@.\minilua.exe
@if errorlevel 8 goto :X64
@set DASC=vm_x86.dasc
@set DASMFLAGS=-D WIN -D JIT -D FFI
@set LJARCH=x86
@set LJCOMPILE=%LJCOMPILE% /arch:SSE2
:X64
@if "%1" neq "nogc64" goto :GC64
@shift
@set DASC=vm_x86.dasc
@set LJCOMPILE=%LJCOMPILE% /DLUAJIT_DISABLE_GC64
:GC64
.\minilua.exe %DASM% -LN %DASMFLAGS% -o host\buildvm_arch.h %DASC%
@if errorlevel 1 goto :BAD

%LJCOMPILE% /I "." /I %DASMDIR% host\buildvm*.cpp
@if errorlevel 1 goto :BAD
"%LJLINK%" %LJLINK_ARGS% /out:buildvm.exe buildvm*.obj
@if errorlevel 1 goto :BAD
if exist buildvm.exe.manifest^
  %LJMT% -manifest buildvm.exe.manifest -outputresource:buildvm.exe

.\buildvm.exe -m peobj -o lj_vm.obj
@if errorlevel 1 goto :BAD
.\buildvm.exe -m bcdef -o lj_bcdef.h %ALL_LIB%
@if errorlevel 1 goto :BAD
.\buildvm.exe -m ffdef -o lj_ffdef.h %ALL_LIB%
@if errorlevel 1 goto :BAD
.\buildvm.exe -m libdef -o lj_libdef.h %ALL_LIB%
@if errorlevel 1 goto :BAD
.\buildvm.exe -m recdef -o lj_recdef.h %ALL_LIB%
@if errorlevel 1 goto :BAD
.\buildvm.exe -m vmdef -o jit\vmdef.lua %ALL_LIB%
@if errorlevel 1 goto :BAD
.\buildvm.exe -m folddef -o lj_folddef.h lj_opt_fold.cpp
@if errorlevel 1 goto :BAD

@if "%1" neq "debug" goto :NODEBUG
@shift
@set BUILDTYPE=debug
@set LJCOMPILE=%LJCOMPILE% /Zi %DEBUGCFLAGS%
@set LJLINK=%LJLINK% /opt:ref /opt:icf /incremental:no
:NODEBUG
@set LJLINK=%LJLINK% /%BUILDTYPE%
%LJCOMPILE% lj_*.cpp lib_*.cpp
@if errorlevel 1 goto :BAD
%LJLIB% /OUT:%LJLIBNAME% lj_*.obj lib_*.obj
@if errorlevel 1 goto :BAD

%LJCOMPILE% luajit.cpp
@if errorlevel 1 goto :BAD

@del *.obj *.manifest minilua.exe buildvm.exe
@del host\buildvm_arch.h
@del lj_bcdef.h lj_ffdef.h lj_libdef.h lj_recdef.h lj_folddef.h
@echo.
@echo === Successfully built LuaJIT for Windows/%LJARCH% ===
exit 0

:BAD
@echo.
@echo *******************************************************
@echo *** Build FAILED -- Please check the error messages ***
@echo *******************************************************
@goto :END
:FAIL
@echo You must open a "Visual Studio Command Prompt" to run this script
:END
