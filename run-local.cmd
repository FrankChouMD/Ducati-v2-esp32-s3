@echo off
setlocal
pushd "%~dp0"
set "DUCATI_PREVIEW_CMAKE=cmake"
where cmake >nul 2>nul
if errorlevel 1 (
    set "DUCATI_PREVIEW_CMAKE=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)
set "MSBUILDDISABLENODEREUSE=1"
echo Building the Ducati V2 local LVGL simulator...
"%DUCATI_PREVIEW_CMAKE%" -S . -B build/native-480 -G "Visual Studio 17 2022" -A x64 -DDUCATI_CANVAS_SIZE=480
if errorlevel 1 goto failed
"%DUCATI_PREVIEW_CMAKE%" --build build/native-480 --target ducati_preview --config Release --parallel 1 -- /nodeReuse:false
if errorlevel 1 goto failed
if /I "%~1"=="--check" (
    start "" /wait "%~dp0build\native-480\Release\ducati_preview.exe" --self-test
    if errorlevel 1 goto failed
    echo Local simulator self-test passed.
    popd
    exit /b 0
)
start "" "%~dp0build\native-480\Release\ducati_preview.exe"
popd
exit /b 0
:failed
echo.
echo Build failed. Visual Studio 2022 C++ tools and Windows SDK are required.
pause
popd
exit /b 1
