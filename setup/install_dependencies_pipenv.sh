#!/bin/bash
# Gesture Car Project - Dependency Installation Script using Pipenv
# This script sets up all Python dependencies using pipenv (Pipfile)

set -e  # Exit on error

echo "========================================"
echo "Gesture Car Project - Setup Script (Pipenv)"
echo "========================================"
echo ""

# Check if Python is installed
echo "Checking Python installation..."
if ! command -v python3 &> /dev/null; then
    echo "[ERROR] Python 3 is not installed or not in PATH!"
    echo ""
    echo "Please install Python 3.7 or higher:"
    echo "  Ubuntu/Debian: sudo apt-get install python3 python3-pip python3-venv"
    echo "  macOS: brew install python3"
    echo "  Or download from: https://www.python.org/downloads/"
    exit 1
fi

# Get Python version
PYTHON_VERSION=$(python3 --version 2>&1 | awk '{print $2}')
echo "[OK] Found: Python $PYTHON_VERSION"

# Check Python version (need 3.7+, recommend 3.10 for mediapipe)
IFS='.' read -ra VERSION_PARTS <<< "$PYTHON_VERSION"
MAJOR_VERSION=${VERSION_PARTS[0]}
MINOR_VERSION=${VERSION_PARTS[1]}

if [ "$MAJOR_VERSION" -lt 3 ] || ([ "$MAJOR_VERSION" -eq 3 ] && [ "$MINOR_VERSION" -lt 7 ]); then
    echo "[ERROR] Python 3.7 or higher is required!"
    echo "  Current version: Python $MAJOR_VERSION.$MINOR_VERSION"
    exit 1
fi

if [ "$MAJOR_VERSION" -gt 3 ] || ([ "$MAJOR_VERSION" -eq 3 ] && [ "$MINOR_VERSION" -gt 12 ]); then
    echo "[WARNING] Python version is over 3.12!"
    echo "  Current version: Python $MAJOR_VERSION.$MINOR_VERSION"
    echo ""
    echo "MediaPipe may not be compatible with Python versions above 3.12."
    echo "It is recommended to install Python 3.10 for best compatibility."
    echo ""
    read -p "Do you want to continue anyway? (y/N): " CONTINUE
    if [[ ! "$CONTINUE" =~ ^[Yy]$ ]]; then
        echo "Installation cancelled. Please install Python 3.10 and try again."
        exit 1
    fi
    echo "Continuing with current Python version..."
    echo ""
fi

# Check if pipenv is installed
echo "Checking pipenv installation..."
if ! command -v pipenv &> /dev/null; then
    echo "pipenv is not installed. Installing pipenv..."
    pip3 install --user pipenv
    if [ $? -ne 0 ]; then
        echo "[ERROR] Failed to install pipenv!"
        echo ""
        echo "You can install it manually with:"
        echo "  pip3 install --user pipenv"
        echo "  Or: sudo pip3 install pipenv"
        exit 1
    fi
    
    # Add pipenv to PATH if installed with --user
    if [ -d "$HOME/.local/bin" ]; then
        export PATH="$HOME/.local/bin:$PATH"
        echo "[OK] Added ~/.local/bin to PATH for this session"
    fi
    
    echo "[OK] pipenv installed"
else
    echo "[OK] pipenv found: $(pipenv --version)"
fi
echo ""

# Navigate to project root (where Pipfile should be)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

if [ ! -f "$PROJECT_ROOT/Pipfile" ]; then
    echo "[ERROR] Pipfile not found in project root!"
    echo "  Expected location: $PROJECT_ROOT/Pipfile"
    exit 1
fi

cd "$PROJECT_ROOT"
echo "[OK] Changed to project root: $PROJECT_ROOT"
echo ""

# Check if virtual environment already exists
if [ -d "$PROJECT_ROOT/.venv" ] || [ -f "$PROJECT_ROOT/.venv" ]; then
    echo "Virtual environment already exists."
    read -p "Do you want to recreate it? (y/N): " RECREATE
    if [[ "$RECREATE" =~ ^[Yy]$ ]]; then
        echo "Removing existing virtual environment..."
        pipenv --rm 2>/dev/null || rm -rf "$PROJECT_ROOT/.venv"
        echo "[OK] Removed old virtual environment"
    else
        echo "Using existing virtual environment."
    fi
fi

# Install dependencies using pipenv
echo "Installing Python dependencies from Pipfile..."
pipenv install
if [ $? -ne 0 ]; then
    echo "[ERROR] Failed to install dependencies!"
    exit 1
fi
echo "[OK] All Python dependencies installed successfully"
echo ""

# Summary
echo ""
echo "========================================"
echo "Setup Complete!"
echo "========================================"
echo ""
echo "Python dependencies have been installed successfully using pipenv."
echo ""
echo "To use the project:"
echo "  1. Activate the pipenv shell:"
echo "     cd /path/to/final-project-gesture-car"
echo "     pipenv shell"
echo ""
echo "  2. Or run commands with pipenv:"
echo "     cd /path/to/final-project-gesture-car"
echo "     pipenv run python Hand_Tracking/Hand_Tracker.py"
echo ""
echo "  3. For Arduino/ESP32 code:"
echo "     - Install Arduino IDE from: https://www.arduino.cc/en/software"
echo "     - Or install PlatformIO for VS Code"
echo "     - Install ESP32 board support in Arduino IDE:"
echo "       File > Preferences > Additional Board Manager URLs"
echo "       Add: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"
echo "       Then: Tools > Board > Boards Manager > Search 'ESP32' > Install"
echo ""
echo "  4. Update the serial port in Hand_Tracker.py or constant.py:"
echo "     Change the port (e.g., '/dev/ttyUSB0' or '/dev/ttyACM0')"
echo "     Find your port with: ls /dev/tty* | grep -E '(USB|ACM)'"
echo ""

