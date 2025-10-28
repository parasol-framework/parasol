@rem Script to generate LuaJIT build tools and headers with MSVC.
@rem This only builds the code generation tools (minilua, buildvm) and generates headers.
@rem The actual library compilation is handled by CMake for proper dependency tracking.
@rem
@rem Usage: msvcbuild_codegen.bat [output_directory]
@rem   If output_directory is specified, generated files go there (default: current dir)

@if not defined INCLUDE goto :FAIL

@rem Check for custom linker before @setlocal
@rem Find MSVC linker in the same directory as cl.exe to avoid Git's link.exe
@if not defined LJLINK (
   for %%i in (cl.exe) do set "LJLINK=%%~dp$PATH:ilink.exe"
)
@if not defined LJLINK_ARGS set "LJLINK_ARGS=/nologo"

@setlocal
@set LJCOMPILE=cl /nologo /c /O2 /W3 /DLUAJIT_ENABLE_LUA52COMPAT /D_CRT_SECURE_NO_DEPRECATE /D_CRT_STDIO_INLINE=__declspec(dllexport)__inline
@set LJMT=mt /nologo
@set DASMDIR=..\dynasm
@set DASM=%DASMDIR%\dynasm.lua
@set DASC=vm_x64.dasc
@set ALL_LIB=lib_base.c lib_math.c lib_bit.c lib_string.c lib_table.c lib_io.c lib_os.c lib_package.c lib_debug.c lib_jit.c lib_ffi.c lib_buffer.c

@rem Set output directory (default to current directory if not specified)
@if "%~1"=="" (
   set "OUTDIR=."
) else (
   set "OUTDIR=%~1"
   if not exist "%OUTDIR%" mkdir "%OUTDIR%"
   if not exist "%OUTDIR%\jit" mkdir "%OUTDIR%\jit"
)

@rem Build minilua only if it doesn't exist (bootstrap tool for code generation)
if not exist minilua.exe goto :BUILD_MINILUA
goto :MINILUA_DONE
:BUILD_MINILUA
%LJCOMPILE% host\minilua.c
@if errorlevel 1 goto :BAD
"%LJLINK%" %LJLINK_ARGS% /out:minilua.exe minilua.obj
@if errorlevel 1 goto :BAD
if exist minilua.exe.manifest^
  %LJMT% -manifest minilua.exe.manifest -outputresource:minilua.exe
@del minilua.obj *.manifest 2>nul
:MINILUA_DONE

@rem Detect architecture
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

@rem Generate buildvm_arch.h (always regenerate as it depends on DASC selection)
.\minilua.exe %DASM% -LN %DASMFLAGS% -o host\buildvm_arch.h %DASC%
@if errorlevel 1 goto :BAD

@rem Build buildvm only if it doesn't exist
if not exist buildvm.exe goto :BUILD_BUILDVM
goto :BUILDVM_DONE
:BUILD_BUILDVM
%LJCOMPILE% /I "." /I %DASMDIR% host\buildvm*.c
@if errorlevel 1 goto :BAD
"%LJLINK%" %LJLINK_ARGS% /out:buildvm.exe buildvm*.obj
@if errorlevel 1 goto :BAD
if exist buildvm.exe.manifest^
  %LJMT% -manifest buildvm.exe.manifest -outputresource:buildvm.exe
@del buildvm*.obj *.manifest 2>nul
:BUILDVM_DONE

@rem Generate VM object and headers
.\buildvm.exe -m peobj -o "%OUTDIR%\lj_vm.obj"
@if errorlevel 1 goto :BAD
.\buildvm.exe -m bcdef -o "%OUTDIR%\lj_bcdef.h" %ALL_LIB%
@if errorlevel 1 goto :BAD
.\buildvm.exe -m ffdef -o "%OUTDIR%\lj_ffdef.h" %ALL_LIB%
@if errorlevel 1 goto :BAD
.\buildvm.exe -m libdef -o "%OUTDIR%\lj_libdef.h" %ALL_LIB%
@if errorlevel 1 goto :BAD
.\buildvm.exe -m recdef -o "%OUTDIR%\lj_recdef.h" %ALL_LIB%
@if errorlevel 1 goto :BAD
.\buildvm.exe -m vmdef -o "%OUTDIR%\jit\vmdef.lua" %ALL_LIB%
@if errorlevel 1 goto :BAD
.\buildvm.exe -m folddef -o "%OUTDIR%\lj_folddef.h" lj_opt_fold.c
@if errorlevel 1 goto :BAD

@rem Clean up temporary build artifacts (keep minilua.exe and buildvm.exe for incremental builds)
@del host\buildvm_arch.h 2>nul
@echo.
@echo === Successfully generated LuaJIT headers for Windows/%LJARCH% in %OUTDIR% ===
exit /b 0

:BAD
@echo.
@echo *******************************************************
@echo *** Build FAILED -- Please check the error messages ***
@echo *******************************************************
@goto :END
:FAIL
@echo You must open a "Visual Studio Command Prompt" to run this script
:END
