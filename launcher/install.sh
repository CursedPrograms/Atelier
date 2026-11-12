#!/usr/bin/env bash
# Installs the Atelier launcher from Git Bash / MSYS on Windows, through install.bat.
set -e
cd "$(dirname "$0")"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) cmd //c install.bat "$@" ;;
    *) echo "The Atelier launcher is for Windows."; exit 1 ;;
esac
