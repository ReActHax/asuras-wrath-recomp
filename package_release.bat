@echo off
rem Asura's Wrath Recompiled - build, deploy to out\dist and create the release zip.
rem Run from "Developer Command Prompt for VS 2022" (or "x64 Native Tools").
rem The game files (extracted\) are never copied into the release.
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
  echo [!] cmake introuvable : lancez ce script depuis "Developer Command Prompt for VS 2022".
  exit /b 1
)

echo [+] Configuration...
cmake --preset win-amd64-release -DREXGLUE_USE_VULKAN=ON -DREXGLUE_USE_D3D12=ON -DREXGLUE_ENABLE_FIDELITYFX=ON
if errorlevel 1 exit /b 1

echo [+] Compilation...
cmake --build out/build/win-amd64-release --parallel
if errorlevel 1 exit /b 1

set "DIST=out\dist\asura_wrath_recomp_windows_x86_64"
if not exist "%DIST%" mkdir "%DIST%"
echo [+] Copie vers %DIST%...
copy /Y "out\build\win-amd64-release\asura_wrath_recomp.exe" "%DIST%\" >nul
if exist "out\build\win-amd64-release\*.dll" copy /Y "out\build\win-amd64-release\*.dll" "%DIST%\" >nul
copy /Y "tools\rexglue\out\win-amd64\*.dll" "%DIST%\" >nul
copy /Y "docs\TEXTURE_MODDING.md" "%DIST%\" >nul
copy /Y "docs\TEXTURE_MODDING.fr.md" "%DIST%\" >nul

echo [+] Launcher...
set "CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if not exist "%CSC%" set "CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe"
"%CSC%" /nologo /codepage:65001 /optimize+ /target:winexe /out:"%DIST%\AsuraLauncher.exe" /r:System.Windows.Forms.dll /r:System.Drawing.dll "tools\launcher\AsuraLauncher.cs"
if errorlevel 1 exit /b 1

echo [+] Archive de publication...
set "REL=out\release\AsurasWrathRecompiled"
if exist "out\release" rmdir /s /q "out\release"
mkdir "%REL%"
copy /Y "%DIST%\asura_wrath_recomp.exe" "%REL%\" >nul
copy /Y "%DIST%\*.dll" "%REL%\" >nul
copy /Y "%DIST%\AsuraLauncher.exe" "%REL%\" >nul
copy /Y "README.md" "%REL%\" >nul
copy /Y "README.fr.md" "%REL%\" >nul
copy /Y "docs\TEXTURE_MODDING.md" "%REL%\" >nul
copy /Y "docs\TEXTURE_MODDING.fr.md" "%REL%\" >nul
copy /Y "asura_wrath_recomp.example.toml" "%REL%\" >nul
powershell -NoProfile -Command "Compress-Archive -Path 'out\release\AsurasWrathRecompiled' -DestinationPath 'out\release\AsurasWrathRecompiled_windows_x64.zip' -Force"
if errorlevel 1 exit /b 1

echo.
echo [+] Termine.
echo     Jeu de developpement : %DIST%
echo     Archive a publier    : out\release\AsurasWrathRecompiled_windows_x64.zip
endlocal
