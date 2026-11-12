# Installs the Atelier launcher for this user (%LOCALAPPDATA%\Programs\Atelier, Start menu + Desktop shortcuts), through install.bat.
& cmd /c "`"$PSScriptRoot\install.bat`" $args"
exit $LASTEXITCODE
