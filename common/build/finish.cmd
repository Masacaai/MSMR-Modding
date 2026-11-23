@echo off
rem Called by every mod's build.cmd after a successful cl: `call "%REPO%common\build\finish.cmd" "%ROOT%"`.
rem Packs <mod>\build\<File>.dll into <File>.script (see pack.ps1 and the mod's mod.json) and, standing rule since 2026-11-19, copies the package to
rem Overstrike's Mods Library so "Install mods" always installs the latest build. Never writes the game's scripts\ folder.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0pack.ps1" -ModDir "%~1." || exit /b 1
set "MODLIB=C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\Mods Library"
for /f "usebackq delims=" %%F in (`powershell -NoProfile -Command "(Get-Content -Raw '%~1mod.json' | ConvertFrom-Json).file"`) do set "MODFILE=%%F"
if exist "%MODLIB%" copy /y "%~1build\%MODFILE%.script" "%MODLIB%\" >nul && echo Copied %MODFILE%.script to the Mods Library
echo BUILD OK
exit /b 0
