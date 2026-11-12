# Builds the Atelier launcher (Release) and dist\Atelier-Launcher-win64.zip, through build.bat (Visual Studio 2022 + CMake).
$ErrorActionPreference = 'Stop'
& cmd /c "`"$PSScriptRoot\build.bat`""
exit $LASTEXITCODE
