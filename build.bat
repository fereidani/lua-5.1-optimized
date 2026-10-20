@echo off
rem build.bat -- Build optimized Lua for Windows (MSVC ABI) with zig cc or clang.
rem
rem Produces, in src\:
rem   lua51.dll   the Lua core and standard libraries
rem   lua51.lib   import library, for linking your own programs against the DLL
rem   lua.exe     the interpreter, using lua51.dll
rem   luac.exe    the bytecode compiler, linked statically (it needs internal
rem               symbols the DLL does not export)
rem
rem Usage:  build.bat          build everything
rem         build.bat clean    remove build outputs
rem
rem The compiler is zig cc if zig is on PATH, otherwise clang. Set CC to force
rem one (e.g. "set CC=clang"), and CFLAGS to replace the default "-O2 -Wall".
rem
rem Everything is built against the MSVC ABI and the shared C runtime, whatever
rem the compiler's default target, so lua51.dll and lua51.lib work from programs
rem built with Visual Studio, and lua.exe and lua51.dll share one heap and one
rem set of stdio buffers. That needs the MSVC libraries and the Windows SDK:
rem install Visual Studio or the Build Tools for Visual Studio. zig and clang
rem find them on their own; a Developer Command Prompt also works. clang must be
rem version 16 or later, for -fms-runtime-lib.

setlocal EnableExtensions
cd /d "%~dp0src" || exit /b 1

set "OUT=lua51.dll lua51.lib lua51.exp lua51.pdb lua.exe lua.lib lua.exp lua.pdb luac.exe luac.lib luac.exp luac.pdb"
if /i "%~1"=="clean" (
    for %%f in (%OUT%) do if exist %%f del /q %%f
    echo Cleaned.
    exit /b 0
)
if not "%~1"=="" (
    echo usage: %~nx0 [clean] 1>&2
    exit /b 1
)

if not defined CC (
    for %%x in (zig.exe) do if not "%%~$PATH:x"=="" set "CC=zig cc"
)
if not defined CC (
    for %%x in (clang.exe) do if not "%%~$PATH:x"=="" set "CC=clang"
)
if not defined CC (
    echo error: neither zig nor clang was found on PATH. 1>&2
    echo        Install one of them, or set CC to a compatible compiler. 1>&2
    exit /b 1
)
if not defined CFLAGS set "CFLAGS=-O2 -Wall"

rem Keep the compiler's architecture and replace the rest of its default triple
rem with windows-msvc. No vendor field: zig rejects "x86_64-pc-windows-msvc".
set "TRIPLE="
for /f "delims=" %%t in ('%CC% -dumpmachine 2^>nul') do set "TRIPLE=%%t"
if not defined TRIPLE (
    echo error: "%CC% -dumpmachine" failed; is CC a working compiler? 1>&2
    exit /b 1
)
for /f "delims=-" %%a in ("%TRIPLE%") do set "ARCH=%%a"
set "TARGET=%ARCH%-windows-msvc"

set "FLAGS=--target=%TARGET% %CFLAGS% -fms-runtime-lib=dll -D_CRT_SECURE_NO_DEPRECATE"

echo Compiler: %CC%
echo Target:   %TARGET%
echo Flags:    %FLAGS%

echo Building lua51.dll...
rem onelua.c compiles the core and libraries as one translation unit, so the
rem compiler can inline across files; see the comment at its top
%CC% %FLAGS% -DLUA_BUILD_AS_DLL -shared -o lua51.dll onelua.c -Wl,/implib:lua51.lib || goto :failed

echo Building lua.exe...
%CC% %FLAGS% -DLUA_BUILD_AS_DLL -o lua.exe lua.c lua51.lib || goto :failed

echo Building luac.exe...
%CC% %FLAGS% -o luac.exe luac.c print.c onelua.c || goto :failed

echo Built: %CD%\lua51.dll lua51.lib lua.exe luac.exe
exit /b 0

:failed
echo error: build failed. 1>&2
exit /b 1
