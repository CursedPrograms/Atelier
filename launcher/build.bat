@echo off
setlocal
cd /d "%~dp0"
rem Builds the Atelier launcher into build\Release, and dist\Atelier-Launcher-win64.zip.
rem Needs Visual Studio 2022 (C++) and CMake. SDL2 is fetched into external\ once (Dear ImGui and stb by CMake).
if not exist "external\SDL2-2.30.9\cmake" (
    echo Getting SDL2's development package, once...
    if not exist external mkdir external
    curl -sL -o external\SDL2-devel.zip https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-devel-2.30.9-VC.zip || goto :error
    tar -xf external\SDL2-devel.zip -C external || goto :error
    del external\SDL2-devel.zip
)
cmake -B build -G "Visual Studio 17 2022" -A x64 || goto :error
cmake --build build --config Release || goto :error
if exist dist\Atelier rmdir /s /q dist\Atelier
mkdir dist\Atelier
for %%f in (Atelier.exe SDL2.dll) do copy /y build\Release\%%f dist\Atelier\ >nul
xcopy /e /i /q /y build\Release\assets dist\Atelier\assets >nul
xcopy /e /i /q /y build\Release\lang dist\Atelier\lang >nul
copy /y ..\LICENSE dist\Atelier\ >nul
copy /y install.bat dist\Atelier\ >nul
copy /y install.ps1 dist\Atelier\ >nul
if exist dist\Atelier-Launcher-win64.zip del dist\Atelier-Launcher-win64.zip
"%SystemRoot%\System32\tar.exe" -a -c -f dist\Atelier-Launcher-win64.zip -C dist Atelier || goto :error
echo Built: build\Release\Atelier.exe and dist\Atelier-Launcher-win64.zip
rem installed on this PC: this build replaces that copy too
if exist "%LOCALAPPDATA%\Programs\Atelier\Atelier.exe" call "%~dp0install.bat" --quiet
exit /b 0
:error
echo Build failed, see above.
pause
exit /b 1
