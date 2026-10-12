@echo off
rem SSB-X: one double-click from your ROM to SSB-X.iso.
rem
rem   1. put your US Super Smash Bros. ROM in this folder, named baserom.z64
rem   2. run this file (double-click, or "build.bat" in a console)
rem
rem It finds a 64-bit Python 3.11-3.14, offering to install Python 3.13
rem with winget if there is none, and hands over to setup.py, which checks
rem everything else the build needs and offers to install what is missing.
rem Arguments are passed on: build.bat --yes, build.bat --check,
rem build.bat --config Debug, build.bat --skip-build.
setlocal EnableExtensions
set "SSBX_SELF=%~f0"
rem A Visual Studio build of an RXDK project runs its Pre-Build Event with no
rem PATH at all; put the Windows folders back so py, where and winget resolve.
if not defined PATH set "PATH=%SystemRoot%\System32;%SystemRoot%;%SystemRoot%\System32\Wbem"
cd /d "%~dp0"
title SSB-X build

echo.
echo  SSB-X -- Super Smash Bros. for the original Xbox
echo.

if not exist "baserom.z64" (
  echo  Your ROM is not here. Copy your US Super Smash Bros. ROM into
  echo      %~dp0
  echo  and name it exactly  baserom.z64  then run this again.
  goto :fail
)

call :findpy
if defined SSBX_PY goto :run

echo  No 64-bit Python 3.11-3.14 was found.
if defined SSBX_NONINTERACTIVE (
  echo  error: Python is needed. Run build.bat once from Explorer to install it, or
  echo  install 64-bit Python 3.13 from https://www.python.org/downloads/
  goto :fail
)
where winget >nul 2>nul
if errorlevel 1 (
  echo  Install 64-bit Python 3.13 from https://www.python.org/downloads/
  echo  ^(tick "Add python.exe to PATH"^), then run this again.
  goto :fail
)
choice /c YN /m " Install Python 3.13 now with winget"
if errorlevel 2 goto :fail
winget install --id Python.Python.3.13 --exact --silent --scope user --accept-package-agreements --accept-source-agreements
call :findpy
if not defined SSBX_PY (
  echo  Python was installed but cannot be found yet. Close this window and
  echo  run build.bat again.
  goto :fail
)

:run
echo  Python: %SSBX_PY%
echo.
"%SSBX_PY%" setup.py %*
set "SSBX_RC=%ERRORLEVEL%"
if not "%SSBX_RC%"=="0" goto :failrc
echo.
echo  Done.
call :pause
exit /b 0

:failrc
echo.
echo  The build stopped (exit code %SSBX_RC%). The messages above say why.
call :pause
exit /b %SSBX_RC%

:fail
echo.
call :pause
exit /b 1

rem ---- the first Python that passes tools\xbox\pycheck.py -------------
:findpy
set "SSBX_PY="
for %%V in (3.13 3.12 3.14 3.11) do (
  if not defined SSBX_PY (
    for /f "delims=" %%P in ('py -%%V-64 tools\xbox\pycheck.py 2^>nul') do set "SSBX_PY=%%P"
  )
)
if defined SSBX_PY exit /b 0
for %%D in (Python313 Python312 Python314 Python311) do (
  if not defined SSBX_PY (
    for %%R in ("%LOCALAPPDATA%\Programs\Python\%%D" "%ProgramFiles%\%%D") do (
      if not defined SSBX_PY if exist "%%~R\python.exe" (
        for /f "delims=" %%P in ('"%%~R\python.exe" tools\xbox\pycheck.py 2^>nul') do set "SSBX_PY=%%P"
      )
    )
  )
)
if defined SSBX_PY exit /b 0
for /f "delims=" %%P in ('python tools\xbox\pycheck.py 2^>nul') do set "SSBX_PY=%%P"
exit /b 0

rem ---- keep a double-clicked window open --------------------------------
:pause
rem (%~0 inside a called label is the label, so the script's path was kept in SSBX_SELF)
if defined SSBX_NONINTERACTIVE exit /b 0
echo %cmdcmdline% | find /i "%SSBX_SELF%" >nul
if not errorlevel 1 pause
exit /b 0
