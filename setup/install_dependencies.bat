@echo off
setlocal enabledelayedexpansion
REM Gesture Car Project - Dependency Installation Script (Batch File)
REM This script sets up all Python dependencies needed for the project

echo ========================================
echo Gesture Car Project - Setup Script
echo ========================================
echo.

REM Check if Python is installed
echo Checking Python installation...
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python is not installed or not in PATH!
    echo.
    echo Please install Python 3.7 or higher from:
    echo   https://www.python.org/downloads/
    echo.
    echo Make sure to check 'Add Python to PATH' during installation.
    pause
    exit /b 1
)

python --version
echo [OK] Python found
echo.

REM Check Python version (need 3.7+, recommend 3.12 for mediapipe)
for /f "tokens=2" %%v in ('python --version 2^>^&1') do set PYTHON_FULL_VERSION=%%v
for /f "tokens=1,2 delims=." %%a in ("%PYTHON_FULL_VERSION%") do (
    set MAJOR_VER=%%a
    set MINOR_VER=%%b
)

REM Check if version is over 3.12
if "%MAJOR_VER%"=="3" (
    if %MINOR_VER% GTR 12 (
        echo [WARNING] Python version is over 3.12!
        echo   Current version: Python %MAJOR_VER%.%MINOR_VER%
        echo.
        echo MediaPipe may not be compatible with Python versions above 3.12.
        echo It is recommended to install Python 3.12 for best compatibility.
        echo.
        echo Download Python 3.12 from:
        echo   https://www.python.org/downloads/release/python-3120/
        echo.
        set /p CONTINUE="Do you want to continue anyway? (y/N): "
        if /i not "!CONTINUE!"=="y" (
            echo Installation cancelled. Please install Python 3.12 and try again.
            pause
            exit /b 1
        )
        echo Continuing with current Python version...
        echo.
    )
)

REM Navigate to the Hand_Tracking directory
REM Script is in setup/ folder, so go up one level to project root
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "HAND_TRACKING_DIR=%PROJECT_ROOT%\Hand_Tracking"

if not exist "%HAND_TRACKING_DIR%" (
    echo [ERROR] Directory 'Hand_Tracking' not found!
    echo Make sure you're running this script from the project root.
    pause
    exit /b 1
)

cd /d "%PROJECT_ROOT%"
echo [OK] Changed to directory: %PROJECT_ROOT%
echo.

REM Check if virtual environment exists
set "VENV_PATH=%HAND_TRACKING_DIR%\venv"
if exist "%VENV_PATH%" (
    echo Virtual environment already exists.
    set /p RECREATE="Do you want to recreate it? (y/N): "
    if /i "%RECREATE%"=="y" (
        echo Removing existing virtual environment...
        rmdir /s /q "%VENV_PATH%"
        echo [OK] Removed old virtual environment
    ) else (
        echo Using existing virtual environment.
    )
)

REM Create virtual environment if it doesn't exist
if not exist "%VENV_PATH%" (
    echo Creating virtual environment...
    python -m venv venv
    if errorlevel 1 (
        echo [ERROR] Failed to create virtual environment!
        pause
        exit /b 1
    )
    echo [OK] Virtual environment created
)

REM Activate virtual environment
echo Activating virtual environment...
call "%VENV_PATH%\Scripts\activate.bat"
if errorlevel 1 (
    echo [ERROR] Failed to activate virtual environment!
    pause
    exit /b 1
)
echo [OK] Virtual environment activated
echo.

REM Upgrade pip
echo Upgrading pip...
python -m pip install --upgrade pip --quiet
if errorlevel 1 (
    echo [ERROR] Failed to upgrade pip!
    pause
    exit /b 1
)
echo [OK] pip upgraded
echo.

REM Install dependencies
echo Installing Python dependencies...
set "REQUIREMENTS_FILE=%PROJECT_ROOT%\requirements.txt"
if exist "%REQUIREMENTS_FILE%" (
    pip install -r "%REQUIREMENTS_FILE%"
    if errorlevel 1 (
        echo [ERROR] Failed to install dependencies!
        pause
        exit /b 1
    )
    echo [OK] All Python dependencies installed successfully
) else (
    echo [ERROR] requirements.txt not found in project root!
    echo Expected location: %REQUIREMENTS_FILE%
    pause
    exit /b 1
)

REM Summary
echo.
echo ========================================
echo Setup Complete!
echo ========================================
echo.
echo Python dependencies have been installed successfully.
echo.
echo To use the project:
echo   1. Activate the virtual environment:
echo      cd Hand_Tracking
echo      venv\Scripts\activate.bat
echo.
echo   2. Run the hand tracker:
echo      python Hand_Tracker.py
echo.
echo   3. For Arduino/ESP32 code:
echo      - Install Arduino IDE from: https://www.arduino.cc/en/software
echo      - Or install PlatformIO for VS Code
echo      - Install ESP32 board support in Arduino IDE:
echo        File ^> Preferences ^> Additional Board Manager URLs
echo        Add: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
echo        Then: Tools ^> Board ^> Boards Manager ^> Search 'ESP32' ^> Install
echo.
echo   4. Update the COM port in Hand_Tracker.py:
echo      Change 'COM11' to your actual COM port
echo.
pause

