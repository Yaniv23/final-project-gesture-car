#!/bin/bash
# Script to launch ESP32 stream and hand tracker in separate terminals
# For Cursor: This script displays commands to execute in separate terminals

# Get script directory (project root folder)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=========================================="
echo "Gesture Control System Launcher"
echo "=========================================="
echo ""
echo "To create visible terminals in Cursor:"
echo "1. Open a new terminal in Cursor (Ctrl+Shift+\` or Terminal > New Terminal)"
echo "2. Execute the following command in this terminal:"
echo ""
echo "   cd '$SCRIPT_DIR' && ./ESP_Camera_Module/start_stream.sh"
echo ""
echo "3. Open a SECOND terminal in Cursor"
echo "4. Execute the following command in this terminal:"
echo ""
echo "   cd '$SCRIPT_DIR' && python3 Hand_Tracking/Hand_Tracker.py"
echo ""
echo "=========================================="
echo ""
read -p "Do you want me to launch the commands now? (y/n) " -n 1 -r
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    echo "[INFO] Launching applications..."
    echo "[INFO] Terminals should appear in Cursor"
    echo ""
    
    # Try to use gnome-terminal or a graphical terminal if available
    if command -v gnome-terminal &> /dev/null; then
        echo "[INFO] Using gnome-terminal"
        gnome-terminal --title="ESP32 Camera Stream" --working-directory="$SCRIPT_DIR" -- bash -c "./ESP_Camera_Module/start_stream.sh; exec bash" &
        sleep 1
        gnome-terminal --title="Hand Tracker" --working-directory="$SCRIPT_DIR" -- bash -c "python3 Hand_Tracking/Hand_Tracker.py; exec bash" &
        echo "[OK] Both terminals have been opened"
    elif command -v xterm &> /dev/null; then
        echo "[INFO] Using xterm"
        xterm -T "ESP32 Camera Stream" -e bash -c "cd '$SCRIPT_DIR' && ./ESP_Camera_Module/start_stream.sh; exec bash" &
        sleep 1
        xterm -T "Hand Tracker" -e bash -c "cd '$SCRIPT_DIR' && python3 Hand_Tracking/Hand_Tracker.py; exec bash" &
        echo "[OK] Both terminals have been opened"
    else
        echo "[INFO] No graphical terminal found"
        echo "[INFO] Please manually create two terminals in Cursor"
        echo "[INFO] and execute the commands above"
    fi
fi
