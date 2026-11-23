@echo off
rem Called by every mod's build.cmd (`call "%~dp0..\common\build\setup.cmd"`; not a standalone script).
rem Loads the Visual Studio x64 toolchain, locates the repository root and the MinHook install shared by all mods, and installs MinHook through
rem vcpkg the first time (manifest: vcpkg.json at the repository root; output: vcpkg_installed\ at the root, git-ignored).
rem Leaves behind, for the caller's cl line:  REPO (repository root, trailing backslash)  VCP (vcpkg triplet dir: include\ and lib\)
for %%I in ("%~dp0..\..") do set "REPO=%%~fI\"
set "VS=C:\Program Files\Microsoft Visual Studio\2022\Community"
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "VCP=%REPO%vcpkg_installed\x64-windows-static"
if not exist "%VCP%\lib\minhook.x64.lib" (
    "%VS%\VC\vcpkg\vcpkg.exe" install --triplet x64-windows-static --x-manifest-root="%REPO%." --x-install-root="%REPO%vcpkg_installed" || exit /b 1
)
exit /b 0
