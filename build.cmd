@echo off
if not exist .run_once_flag (
    rem You can put commands here that should execute only on the first run.
    if exist build (rmdir /s /q build)
    echo.> .run_once_flag
)
rem then build
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=c:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build
@echo on