#!/usr/bin/env bash
# Builds the Atelier launcher from Git Bash / MSYS on Windows, through build.bat (Visual Studio 2022 + CMake).
# The launcher is Windows only: it installs the suite's Windows releases.
set -e
cd "$(dirname "$0")"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) cmd //c build.bat ;;
    *) echo "The Atelier launcher is for Windows (the suite's releases are Windows builds)."; exit 1 ;;
esac
