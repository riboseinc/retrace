@echo off
rem trace-profile-quickstart: Windows flow (TODO.trace-profile/16).
rem   capture (delegates to retrace-win-run) -> diff -> jail -> jailed run
rem usage: run-windows.bat [path-to-build-dir]
setlocal enabledelayedexpansion
set BUILD=%1
if "%BUILD%"=="" set BUILD=..\..\build
set TOOLS=%BUILD%\tools

echo declared> declared.dat
echo secret> undeclared.dat
echo brand new> new-feature.dat

cl /O1 /Fe:app.exe "%~dp0app.c" >nul 2>&1 || (
  echo cannot build app.c with cl -- run from a VS developer prompt
  exit /b 1
)

echo === 1. capture (baseline; capture finds retrace-win-run + retrace.dll)
"%TOOLS%\retrace-profile" capture -o baseline.json -- app.exe "%CD%"
if errorlevel 2 goto :err

echo === 2. validate
"%TOOLS%\retrace-profile" validate baseline.json

echo === 3. diff the "upgrade"
"%TOOLS%\retrace-profile" capture -o candidate.json -- app.exe "%CD%" upgraded
"%TOOLS%\retrace-profile" diff baseline.json candidate.json
if errorlevel 2 goto :err

echo === 4. jail from the candidate profile (declared set only)
set ESC=%CD:\=\\%
> inside.json (
  echo {"profile":{"functions":[{"name":"fopen","count":1}],
  echo  "accesses":[{"path":"%ESC%/declared.dat","class":"read","hits":1}]}}
)
"%TOOLS%\retrace-profile" jail candidate.json --inside inside.json -o jail.json
if errorlevel 2 goto :err

echo === 5. run under the jail (undeclared.dat must be DENIED)
set RETRACE_JSON_CONFIG=jail.json
"%TOOLS%\retrace-win-run" app.exe "%CD%"
set RETRACE_JSON_CONFIG=

echo === done
exit /b 0
:err
echo FAILED (see output above)
exit /b 1
