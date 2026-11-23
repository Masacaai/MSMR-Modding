@echo off
rem Builds EchoesOfTime.dll and EchoesOfTime.script into build\ (shared toolchain, packing and Mods Library copy: common\build\)
setlocal
set ROOT=%~dp0
call "%ROOT%..\common\build\setup.cmd" || exit /b 1
if not exist "%ROOT%build" mkdir "%ROOT%build"

cl /nologo /LD /O2 /MT /EHa /std:c++20 /W3 /Zi /DNDEBUG /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I"%VCP%\include" /I"%REPO%common" /I"%REPO%common\game" /I"%REPO%common\modmenu" /I"%ROOT%src" /Fo"%ROOT%build\\" /Fd"%ROOT%build\\" ^
   "%ROOT%src\*.cpp" "%REPO%common\*.cpp" "%REPO%common\modmenu\*.cpp" "%REPO%common\game\*.cpp" /Fe"%ROOT%build\EchoesOfTime.dll" ^
   /link /LIBPATH:"%VCP%\lib" minhook.x64.lib user32.lib shell32.lib hid.lib setupapi.lib /DEBUG /MAP /OPT:REF /OPT:ICF || exit /b 1

call "%REPO%common\build\finish.cmd" "%ROOT%"
