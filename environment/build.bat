@echo off
setlocal

set "CUBE_EXE="
for /d %%D in ("%USERPROFILE%\.vscode\extensions\stmicroelectronics.stm32cube-ide-core-*") do (
    if exist "%%~fD\resources\binaries\win32\x86_64\cube.exe" set "CUBE_EXE=%%~fD\resources\binaries\win32\x86_64\cube.exe"
    if defined CUBE_EXE goto :found_cube
)

:found_cube
set "CUBECLT_ROOT="
for /d %%D in (C:\ST\STM32CubeCLT_*) do (
    set "CUBECLT_ROOT=%%~fD"
    goto :found_clt
)

:found_clt
if defined CUBECLT_ROOT (
    set "NINJA_EXE=%CUBECLT_ROOT%\Ninja\bin\ninja.exe"
    if exist "%CUBECLT_ROOT%\Ninja\bin\ninja.exe" set "PATH=%CUBECLT_ROOT%\Ninja\bin;%PATH%"
    if exist "%CUBECLT_ROOT%\st-arm-clang\bin\starm-clang.exe" set "PATH=%CUBECLT_ROOT%\st-arm-clang\bin;%PATH%"
)

if exist build rmdir /s /q build
if defined CUBE_EXE if exist "%CUBE_EXE%" goto :configure_cube
if defined NINJA_EXE if exist "%NINJA_EXE%" goto :configure_ninja
cmake --preset=Debug .
goto :build_cmake

:configure_cube
"%CUBE_EXE%" cmake --preset=Debug .
"%CUBE_EXE%" cmake --build --preset=Debug
if /i not "%~1"=="--no-pause" pause
exit /b %ERRORLEVEL%

:configure_ninja
cmake --preset=Debug . -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%"

:build_cmake
cmake --build --preset=Debug
if /i not "%~1"=="--no-pause" pause
