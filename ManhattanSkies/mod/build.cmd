@echo off
rem Builds ManhattanSkies.dll and ManhattanSkies.script into build\ (does NOT install anything into the game)
rem MinHook comes from EndlessArena's vcpkg install (same triplet), so nothing is downloaded.
setlocal
set ROOT=%~dp0
set VS=C:\Program Files\Microsoft Visual Studio\2022\Community
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

set VCP=%ROOT%..\..\EndlessArena\vcpkg_installed\x64-windows-static
if not exist "%VCP%\lib\minhook.x64.lib" (
    echo MinHook not found at %VCP%. Build EndlessArena once first ^(its build.cmd installs it^).
    exit /b 1
)
if not exist "%ROOT%build" mkdir "%ROOT%build"

cl /nologo /LD /O2 /MT /EHa /std:c++20 /W3 /Zi /DNDEBUG /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I"%VCP%\include" /I"%ROOT%..\..\common" /I"%ROOT%src" /Fo"%ROOT%build\\" /Fd"%ROOT%build\\" ^
   "%ROOT%src\*.cpp" "%ROOT%..\..\common\*.cpp" "%ROOT%..\..\ModMenu\*.cpp" /Fe"%ROOT%build\ManhattanSkies.dll" ^
   /link /LIBPATH:"%VCP%\lib" minhook.x64.lib user32.lib shell32.lib /DEBUG /MAP /OPT:REF /OPT:ICF || exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%pack.ps1" || exit /b 1
echo BUILD OK
