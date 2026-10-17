@echo off
rem Builds VST3 + CLAP + Standalone for Windows x64.
rem Requires: Visual Studio 2022 (or newer) with "Desktop development with C++", CMake 3.22+, Git.
cmake -B build -A x64 || exit /b 1
cmake --build build --config Release --parallel || exit /b 1
echo.
echo Plugins are in build\MetalSubDrop_artefacts\Release\
echo   VST3: copy "Metal Sub Drop.vst3" to C:\Program Files\Common Files\VST3\
echo   CLAP: copy "Metal Sub Drop.clap" to C:\Program Files\Common Files\CLAP\
