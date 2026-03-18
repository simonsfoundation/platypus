@echo off
setlocal enabledelayedexpansion

REM Platypus build configuration script for Windows
REM Clones vcpkg, installs dependencies, and configures CMake
REM
REM Options:
REM   --update-vcpkg    Pull latest vcpkg and re-bootstrap
REM   --clean-cache     Clear the vcpkg binary cache

set "SCRIPT_DIR=%~dp0"
set "VCPKG_DIR=%SCRIPT_DIR%vcpkg"
set "BUILD_DIR=%SCRIPT_DIR%build"

REM Persistent binary cache — survives clean builds
set "VCPKG_CACHE_DIR=%LOCALAPPDATA%\vcpkg\archives"

REM ---------- Parse flags ----------
set "UPDATE_VCPKG=0"
set "CLEAN_CACHE=0"
for %%A in (%*) do (
    if "%%A"=="--update-vcpkg" set "UPDATE_VCPKG=1"
    if "%%A"=="--clean-cache" set "CLEAN_CACHE=1"
)

REM ---------- Check prerequisites ----------
where git >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: git is not installed or not on PATH.
    echo   Install Git from https://git-scm.com/
    exit /b 1
)

where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: cmake is not installed or not on PATH.
    echo   Install CMake from https://cmake.org/download/
    exit /b 1
)

where ninja >nul 2>&1
if %errorlevel% neq 0 (
    echo WARNING: ninja is not found on PATH.
    echo   The build will use the default generator instead of Ninja.
    set "USE_NINJA=0"
) else (
    set "USE_NINJA=1"
)

REM ---------- Set up vcpkg binary cache ----------
if "%CLEAN_CACHE%"=="1" (
    echo Clearing vcpkg binary cache...
    if exist "%VCPKG_CACHE_DIR%" rd /s /q "%VCPKG_CACHE_DIR%"
)
if not exist "%VCPKG_CACHE_DIR%" mkdir "%VCPKG_CACHE_DIR%"
set "VCPKG_DEFAULT_BINARY_CACHE=%VCPKG_CACHE_DIR%"
echo Binary cache: %VCPKG_CACHE_DIR%

REM ---------- Clone and bootstrap vcpkg ----------
if not exist "%VCPKG_DIR%" (
    echo Cloning vcpkg...
    git clone https://github.com/microsoft/vcpkg.git "%VCPKG_DIR%"
    if %errorlevel% neq 0 (
        echo ERROR: Failed to clone vcpkg.
        exit /b 1
    )
) else if "%UPDATE_VCPKG%"=="1" (
    echo Updating vcpkg...
    git -C "%VCPKG_DIR%" pull --ff-only
) else (
    echo Using existing vcpkg ^(pass --update-vcpkg to refresh^).
)

if not exist "%VCPKG_DIR%\vcpkg.exe" (
    echo Bootstrapping vcpkg...
    call "%VCPKG_DIR%\bootstrap-vcpkg.bat" -disableMetrics
    if %errorlevel% neq 0 (
        echo ERROR: Failed to bootstrap vcpkg.
        exit /b 1
    )
) else if "%UPDATE_VCPKG%"=="1" (
    echo Re-bootstrapping vcpkg...
    call "%VCPKG_DIR%\bootstrap-vcpkg.bat" -disableMetrics
)

echo vcpkg is ready.

REM ---------- Configure CMake ----------
echo.
echo Configuring CMake...

set "CMAKE_ARGS=-B "%BUILD_DIR%" -S "%SCRIPT_DIR%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake""

if "%USE_NINJA%"=="1" (
    set "CMAKE_ARGS=%CMAKE_ARGS% -G Ninja"
)

cmake %CMAKE_ARGS%

if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed.
    exit /b 1
)

echo.
echo Configuration complete!
echo.
echo To build:
echo   cmake --build build --target PlatypusGui
echo.
echo To run:
echo   build\PlatypusGui.exe
echo.
echo Subsequent runs will be fast (packages cached at %VCPKG_CACHE_DIR%).

endlocal
