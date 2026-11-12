@echo off
setlocal
cd /d "%~dp0"
rem Installs the Atelier launcher for this user: %LOCALAPPDATA%\Programs\Atelier, with Start menu and Desktop shortcuts.
rem Run build.bat first (or unzip a release and run this beside it).
set "SRC=dist\Atelier"
if exist "Atelier.exe" set "SRC=."
if not exist "%SRC%\Atelier.exe" ( echo Build it first: build.bat & exit /b 1 )
tasklist /fi "imagename eq Atelier.exe" | find /i "Atelier.exe" >nul
if not errorlevel 1 ( echo Atelier is open: close it and run this again. & exit /b 1 )
set "INST=%LOCALAPPDATA%\Programs\Atelier"
if not exist "%INST%" mkdir "%INST%"
xcopy /e /i /q /y "%SRC%\*" "%INST%\" >nul || ( echo Couldn't copy to %INST% & exit /b 1 )
powershell -NoProfile -Command "$s=New-Object -ComObject WScript.Shell; foreach($d in @([Environment]::GetFolderPath('Programs'),[Environment]::GetFolderPath('Desktop'))){ $l=$s.CreateShortcut((Join-Path $d 'Atelier.lnk')); $l.TargetPath='%INST%\Atelier.exe'; $l.WorkingDirectory='%INST%'; $l.IconLocation='%INST%\Atelier.exe,0'; $l.Description='Atelier: install and update the Creative & Studio Suite'; $l.Save() }"
echo Installed: %INST%
if /i not "%~1"=="--quiet" echo Start it from the Start menu or the Desktop.
exit /b 0
