@echo off
setlocal
cd /d "%~dp0"
where cmake >nul 2>nul
if errorlevel 1 (
  echo ERROR: CMake not found. Install Visual Studio 2022 with Desktop development with C++ and CMake tools.
  pause
  exit /b 1
)
if not exist build mkdir build
cmake -S . -B build -A x64
if errorlevel 1 goto failed
cmake --build build --config Release
if errorlevel 1 goto failed
echo.
echo BUILD SUCCESSFUL
 echo EXE: %CD%\build\Release\PS5ExperimentalEmulator.exe
pause
exit /b 0
:failed
echo.
echo BUILD FAILED. Read the errors above.
pause
exit /b 1
