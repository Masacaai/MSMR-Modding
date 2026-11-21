@echo off
rem Builds DisableWebs.dll and DisableWebs.script into build\ and copies the package to Overstrike's Mods Library (never writes scripts\)
rem Shares EndlessArena's game-access code (natives, entity) and common\. MinHook comes from EndlessArena's vcpkg install.
setlocal
set ROOT=%~dp0
set EA=%ROOT%..\EndlessArena
set VS=C:\Program Files\Microsoft Visual Studio\2022\Community
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

set VCP=%EA%\vcpkg_installed\x64-windows-static
if not exist "%VCP%\lib\minhook.x64.lib" (
    echo MinHook not found at %VCP%. Build EndlessArena once first ^(its build.cmd installs it^).
    exit /b 1
)
if not exist "%ROOT%build" mkdir "%ROOT%build"

cl /nologo /LD /O2 /MT /EHa /std:c++20 /W3 /Zi /DNDEBUG /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I"%VCP%\include" /I"%ROOT%..\common" /I"%EA%\src" /I"%ROOT%src" /Fo"%ROOT%build\\" /Fd"%ROOT%build\\" ^
   "%ROOT%src\*.cpp" "%ROOT%..\common\*.cpp" "%EA%\src\natives.cpp" "%EA%\src\entity.cpp" /Fe"%ROOT%build\DisableWebs.dll" ^
   /link /LIBPATH:"%VCP%\lib" minhook.x64.lib user32.lib shell32.lib /DEBUG /MAP /OPT:REF /OPT:ICF || exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%pack.ps1" || exit /b 1
set "MODLIB=C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\Mods Library"
if exist "%MODLIB%" copy /y "%ROOT%build\DisableWebs.script" "%MODLIB%\" >nul && echo Copied DisableWebs.script to the Mods Library
echo BUILD OK
