@echo off
rem Builds and runs the late-chapter checks (tests\sinister_sim.cpp): invariants over many seeds and settings, plus the chance counter's maths.
setlocal
set ROOT=%~dp0..
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul || exit /b 1
if not exist "%ROOT%\build\tests" mkdir "%ROOT%\build\tests"
cl /nologo /O2 /MT /EHa /std:c++20 /Fo"%ROOT%\build\tests\\" "%ROOT%\tests\sinister_sim.cpp" "%ROOT%\src\plan.cpp" "%ROOT%\src\roster.cpp" /Fe"%ROOT%\build\tests\sinister_sim.exe" >nul || exit /b 1
"%ROOT%\build\tests\sinister_sim.exe"
