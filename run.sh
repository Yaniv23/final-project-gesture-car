#!/bin/bash
# Script to run hand tracking and open ESP32 camera stream in browser

# Get the project root directory
PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"

# Default ESP32 IP (can be overridden with --ip argument)
ESP32_IP="${ESP32_IP:-192.168.0.129}"
ESP32_STREAM_URL="http://${ESP32_IP}/stream"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --ip)
            ESP32_IP="$2"
            ESP32_STREAM_URL="http://${ESP32_IP}/stream"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--ip ESP32_IP_ADDRESS]"
            exit 1
            ;;
    esac
done

# Activate virtual environment if it exists
if [ -d "$PROJECT_ROOT/Hand_Tracking/venv" ]; then
    echo "Activating virtual environment..."
    source "$PROJECT_ROOT/Hand_Tracking/venv/bin/activate"
fi

# Change to project root
cd "$PROJECT_ROOT"

echo "=========================================="
echo "Starting Gesture Car Control"
echo "=========================================="
echo "ESP32 Stream URL: $ESP32_STREAM_URL"
echo ""

# Open ESP32 stream in browser
echo "Opening ESP32 camera stream in browser..."
if command -v xdg-open &> /dev/null; then
    # Linux
    xdg-open "$ESP32_STREAM_URL" &
elif command -v open &> /dev/null; then
    # macOS
    open "$ESP32_STREAM_URL" &
elif command -v start &> /dev/null; then
    # Windows (Git Bash)
    start "$ESP32_STREAM_URL" &
else
    echo "Could not detect browser command. Please open manually:"
    echo "  $ESP32_STREAM_URL"
fi

# Wait a moment for browser to open
sleep 2

# Run hand tracking
echo "Starting hand tracking..."
echo "Press 'q' in the hand tracking window to quit"
echo ""
python "$PROJECT_ROOT/Hand_Tracking/Hand_Tracker.py"

echo ""
echo "Shutting down..."


