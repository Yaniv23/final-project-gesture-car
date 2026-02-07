#!/bin/bash
# Script to launch ESP32 stream and hand tracker in separate terminals
# Terminals will automatically close when their respective windows are closed
#
# Usage:
#   ./launch.sh              Launch both camera and hand tracker (default)
#   ./launch.sh --cam-only   Launch only ESP32 camera (MJPEG viewer)
#   ./launch.sh --hand-only  Launch only hand tracker
#   ./launch.sh --help       Show this help

# Get script directory (project root folder)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# What to launch: 1 = launch, 0 = skip (default: both)
LAUNCH_CAM=1
LAUNCH_HAND=1

# Parse options
for arg in "$@"; do
    case "$arg" in
        --cam-only)
            LAUNCH_CAM=1
            LAUNCH_HAND=0
            ;;
        --hand-only)
            LAUNCH_CAM=0
            LAUNCH_HAND=1
            ;;
        --help|-h)
            echo "Usage: $0 [OPTION]"
            echo ""
            echo "  (no options)   Launch both camera and hand tracker"
            echo "  --cam-only     Launch only ESP32 camera (MJPEG viewer)"
            echo "  --hand-only    Launch only hand tracker"
            echo "  --help, -h     Show this help"
            exit 0
            ;;
        *)
            echo "[ERROR] Unknown option: $arg" >&2
            echo "Use --help for usage." >&2
            exit 1
            ;;
    esac
done

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

# Launch requested terminals
if command -v gnome-terminal &> /dev/null; then
    echo "[INFO] Using gnome-terminal"
    if [ "$LAUNCH_CAM" -eq 1 ]; then
        gnome-terminal --title="ESP32 Camera Stream" \
            --working-directory="$SCRIPT_DIR" \
            -- bash -c "python3 pc_side/ESP-CAM/mjpeg_viewer.py; exit" &
        TERMINAL_PIDS+=($!)
        [ "$LAUNCH_HAND" -eq 1 ] && sleep 1
    fi
    if [ "$LAUNCH_HAND" -eq 1 ]; then
        gnome-terminal --title="Hand Tracker" \
            --working-directory="$SCRIPT_DIR/pc_side/Hand_Tracking" \
            -- bash -c "python3 Hand_Tracker.py; exit" &
        TERMINAL_PIDS+=($!)
    fi
elif command -v xterm &> /dev/null; then
    echo "[INFO] Using xterm"
    if [ "$LAUNCH_CAM" -eq 1 ]; then
        xterm -T "ESP32 Camera Stream" -e bash -c "cd '$SCRIPT_DIR' && python3 pc_side/ESP-CAM/mjpeg_viewer.py; exit" &
        TERMINAL_PIDS+=($!)
        [ "$LAUNCH_HAND" -eq 1 ] && sleep 1
    fi
    if [ "$LAUNCH_HAND" -eq 1 ]; then
        xterm -T "Hand Tracker" -e bash -c "cd '$SCRIPT_DIR/pc_side/Hand_Tracking' && python3 Hand_Tracker.py; exit" &
        TERMINAL_PIDS+=($!)
    fi
else
    echo "[ERROR] No graphical terminal found (gnome-terminal or xterm required)"
    exit 1
fi

if [ "$LAUNCH_CAM" -eq 1 ] && [ "$LAUNCH_HAND" -eq 1 ]; then
    echo "[OK] Both terminals have been opened"
else
    echo "[OK] Terminal has been opened"
fi
echo "[INFO] Terminals will close automatically when windows are closed"
echo "[INFO] Press Ctrl+C to stop all processes"
echo ""

# Wait for all terminal processes to finish
wait
