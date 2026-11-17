# Installs the Atelier launcher for this user (%LOCALAPPDATA%\Programs\Atelier, Start menu and Desktop shortcuts named "Atelier - Atelier"), through install.bat.
& cmd /c "`"$PSScriptRoot\install.bat`" $args"
exit $LASTEXITCODE
