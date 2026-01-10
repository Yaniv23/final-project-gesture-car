#!/bin/bash
# Script to launch ESP32 stream and hand tracker in separate terminals
# Terminals will automatically close when their respective windows are closed

# Get script directory (project root folder)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Arrays to store terminal PIDs
TERMINAL_PIDS=()

# Cleanup function to close all terminals
cleanup() {
    echo ""
    echo "[INFO] Closing all terminals..."
    for pid in "${TERMINAL_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null
        fi
    done
    exit 0
}

# Trap signals to cleanup on exit
trap cleanup SIGINT SIGTERM EXIT

echo "=========================================="
echo "Gesture Control System Launcher"
echo "=========================================="
echo ""
echo "[INFO] Launching applications..."
echo ""

# Launch ESP32 Camera Stream terminal
if command -v gnome-terminal &> /dev/null; then
    echo "[INFO] Using gnome-terminal"
    # Launch terminal - it will close automatically when the command exits
    gnome-terminal --title="ESP32 Camera Stream" \
        --working-directory="$SCRIPT_DIR" \
        -- bash -c "./ESP_Camera_Module/start_stream.sh; exit" &
    TERMINAL_PIDS+=($!)
    sleep 1
    
    # Launch Hand Tracker terminal
    gnome-terminal --title="Hand Tracker" \
        --working-directory="$SCRIPT_DIR" \
        -- bash -c "python3 Hand_Tracking/Hand_Tracker.py; exit" &
    TERMINAL_PIDS+=($!)
    
elif command -v xterm &> /dev/null; then
    echo "[INFO] Using xterm"
    # xterm closes automatically when command exits
    xterm -T "ESP32 Camera Stream" -e bash -c "cd '$SCRIPT_DIR' && ./ESP_Camera_Module/start_stream.sh; exit" &
    TERMINAL_PIDS+=($!)
    sleep 1
    
    xterm -T "Hand Tracker" -e bash -c "cd '$SCRIPT_DIR' && python3 Hand_Tracking/Hand_Tracker.py; exit" &
    TERMINAL_PIDS+=($!)
    
else
    echo "[ERROR] No graphical terminal found (gnome-terminal or xterm required)"
    exit 1
fi

echo "[OK] Both terminals have been opened"
echo "[INFO] Terminals will close automatically when windows are closed"
echo "[INFO] Press Ctrl+C to stop all processes"
echo ""

# Wait for all terminal processes to finish
wait
