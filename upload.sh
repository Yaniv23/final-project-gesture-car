#!/bin/bash

# Script to upload and monitor ESP32 code for Car or Sender
# Usage: ./upload.sh [car|sender]

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get the script directory (main folder)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Function to print colored messages
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Check if PlatformIO is installed
if ! command -v pio &> /dev/null; then
    print_error "PlatformIO is not installed or not in PATH"
    print_info "Please install PlatformIO: https://platformio.org/install/cli"
    exit 1
fi

# Parse command line argument
BOARD_TYPE="${1:-}"

if [ -z "$BOARD_TYPE" ]; then
    print_error "No board type specified"
    echo ""
    echo "Usage: $0 [car|sender]"
    echo ""
    echo "  car     - Upload and monitor ESP32 Car code"
    echo "  sender  - Upload and monitor ESP32 Sender code"
    exit 1
fi

# Normalize input to lowercase
BOARD_TYPE=$(echo "$BOARD_TYPE" | tr '[:upper:]' '[:lower:]')

# Set project directory based on board type
case "$BOARD_TYPE" in
    car|vehicle|veh)
        PROJECT_DIR="$SCRIPT_DIR/Car"
        BOARD_NAME="Car"
        ;;
    sender|send)
        PROJECT_DIR="$SCRIPT_DIR/pc_side/Sender_Code"
        BOARD_NAME="Sender"
        ;;
    *)
        print_error "Invalid board type: $BOARD_TYPE"
        echo ""
        echo "Valid options: car, sender"
        exit 1
        ;;
esac

# Check if project directory exists
if [ ! -d "$PROJECT_DIR" ]; then
    print_error "Project directory not found: $PROJECT_DIR"
    exit 1
fi

# Check if platformio.ini exists
if [ ! -f "$PROJECT_DIR/platformio.ini" ]; then
    print_error "platformio.ini not found in $PROJECT_DIR"
    exit 1
fi

print_info "Selected board: $BOARD_NAME"
print_info "Project directory: $PROJECT_DIR"
echo ""

# Change to project directory
cd "$PROJECT_DIR"

# Build the project
print_info "Building project..."
if ! pio run; then
    print_error "Build failed!"
    exit 1
fi

print_info "Build successful!"
echo ""

# Upload the code
print_info "Uploading code to $BOARD_NAME..."
if ! pio run -t upload; then
    print_error "Upload failed!"
    exit 1
fi

print_info "Upload successful!"
echo ""

# Start monitor
print_info "Starting serial monitor..."
print_info "Press Ctrl+C to stop monitoring"
echo ""

pio device monitor
