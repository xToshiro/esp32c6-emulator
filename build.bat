@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo    ESP32-C6 HARDWARE ^& SENSOR EMULATOR BUILD SYSTEM
echo ========================================================

:: 1. Locate G++
set GXX=g++
where g++ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    if exist "%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\g++.exe" (
        set "GXX=%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\g++.exe"
    )
)

if not exist "!GXX!" (
    where !GXX! >nul 2>&1
    if %ERRORLEVEL% NEQ 0 (
        echo [ERRO] Compilador G++ nao foi localizado!
        exit /b 1
    )
)

echo [COMPILADOR] Utilizando: !GXX!

:: 2. Compile Test Suites
echo [BUILD] Compilando suite de testes de integracao C++...
"!GXX!" -static -std=c++17 -I. -I./hal ^
    emulator_runner/test_suites.cpp ^
    hal/Arduino.cpp hal/Wire.cpp hal/WiFi.cpp hal/SD.cpp hal/Preferences.cpp ^
    hal/HTTPClient.cpp hal/Adafruit_NeoPixel.cpp hal/ESPmDNS.cpp hal/Update.cpp ^
    -o emulator_runner/test_suites.exe

if %ERRORLEVEL% NEQ 0 (
    echo [ERRO] Falha na compilacao de test_suites.exe!
    exit /b %ERRORLEVEL%
)
echo [OK] emulator_runner/test_suites.exe gerado com sucesso.

:: Locate PlatformIO
set PIO_BIN=pio
where pio >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    if exist "%USERPROFILE%\.platformio\penv\Scripts\pio.exe" (
        set "PIO_BIN=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"
    )
)

:: 3. Run Option
if "%1"=="compile-bin" (
    echo.
    echo [PIO] Compilando firmware .bin nativo ESP32-C6 com PlatformIO...
    if exist "main_v4.0.0.cpp" (
        powershell -Command "Copy-Item main_v4.0.0.cpp src/main.cpp -Force"
    )
    "!PIO_BIN!" run
    if !ERRORLEVEL! EQU 0 (
        echo.
        echo [OK] Binario gerado: .pio\build\esp32-c6-devkitc-1\firmware.bin
        echo [OK] ELF gerado:     .pio\build\esp32-c6-devkitc-1\firmware.elf
        exit /b 0
    ) else (
        echo [ERRO] Falha na compilacao com PlatformIO.
        exit /b !ERRORLEVEL!
    )
)

if "%1"=="test" (
    echo.
    echo [RUN] Executando testes de integracao...
    emulator_runner\test_suites.exe
    exit /b !ERRORLEVEL!
)

if "%1"=="dashboard" (
    echo.
    echo [RUN] Iniciando Web Dashboard na porta 3000...
    node web_dashboard/server.js
    exit /b 0
)

if "%1"=="pdf" (
    echo.
    echo [PDF] Gerando documentacao arquitetural em PDF...
    node build_architecture_pdf.js
    exit /b !ERRORLEVEL!
)

echo.
echo ========================================================
echo  BUILD CONCLUIDO COM SUCESSO!
echo  Para compilar o .bin:      build.bat compile-bin
echo  Para rodar os testes:      build.bat test
echo  Para iniciar o dashboard:  build.bat dashboard
echo  Para gerar o PDF:          build.bat pdf
echo ========================================================
