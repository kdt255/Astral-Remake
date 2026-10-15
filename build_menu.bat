@echo off
rem Astral-Remake - made by kdt255. MIT + Commons Clause (free, not for sale).
setlocal enabledelayedexpansion

if not defined VSCMD_ARG_TGT_ARCH (
  set "VS="
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i\VC\Auxiliary\Build\vcvars64.bat"
  if not exist "!VS!" echo [!] vcvars64.bat not found & exit /b 1
  call "!VS!" >nul
)

cd /d "%~dp0"
if not exist bin mkdir bin
if not exist bin\obj_menu mkdir bin\obj_menu

set IMGUI=third_party\imgui
set SRC=menu\src\main.cpp menu\src\astral\draw.cpp menu\src\astral\icons.cpp menu\src\astral\ui.cpp menu\src\astral\preview.cpp menu\src\astral\persist.cpp menu\src\astral\menu.cpp menu\src\astral\pages.cpp
set SRC=%SRC% %IMGUI%\imgui.cpp %IMGUI%\imgui_draw.cpp %IMGUI%\imgui_tables.cpp %IMGUI%\imgui_widgets.cpp %IMGUI%\backends\imgui_impl_win32.cpp %IMGUI%\backends\imgui_impl_dx11.cpp
set SRC=%SRC% %IMGUI%\misc\freetype\imgui_freetype.cpp
set LIBS=d3d11.lib dxgi.lib d3dcompiler.lib user32.lib gdi32.lib shell32.lib dwmapi.lib third_party\freetype\lib\x64\freetype.lib

cl /nologo /std:c++17 /EHsc /O2 /MD /MP /W4 /utf-8 /permissive- /DNOMINMAX /DWIN32_LEAN_AND_MEAN /DUNICODE /D_UNICODE ^
   /I menu\src /I %IMGUI% /I %IMGUI%\backends /I %IMGUI%\misc\freetype /I third_party\freetype\include /Fobin\obj_menu\ /Febin\astral.exe %SRC% /link /SUBSYSTEM:WINDOWS %LIBS%
if errorlevel 1 (echo. & echo [!] BUILD FAILED & exit /b 1)

echo. & echo [+] bin\astral.exe
