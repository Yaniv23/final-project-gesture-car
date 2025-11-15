# Setup Instructions

Follow these steps to set up the project. Just copy and paste the commands.

## Step 1: Install Dependencies

### Option A: Double-Click (Easiest)

1. Go to the `setup` folder
2. Double-click `install_dependencies.bat`
3. Wait for it to finish

### Option B: PowerShell

1. Open PowerShell
2. Copy and paste this:

```powershell
cd "D:\Final Project\FinalRepo\final-project-gesture-car"
.\setup\install_dependencies.ps1
```

**If you get an error about execution policy, copy and paste this first:**
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

Then try again.

## Step 2: Run the Hand Tracker

1. Copy and paste these commands one by one:

```powershell
cd "D:\Final Project\FinalRepo\final-project-gesture-car\mediapipe_hand_direction"
```

```powershell
.\venv\Scripts\Activate.ps1
```

```powershell
python hand_direction_tracker.py
```

2. A camera window will open. Show your hand to the camera.
3. Press `q` to quit.

## Step 3: Connect ESP32 (Optional)

If you want to control the car:

1. Connect your ESP32 to the computer via USB
2. Open `hand_direction_tracker.py` in a text editor
3. Find line 15 that says: `SERIAL_PORT = 'COM11'`
4. Change `'COM11'` to your ESP32's COM port (see below how to find it)

### How to Find Your COM Port

Copy and paste this in PowerShell:

```powershell
Get-PnpDevice -Class Ports | Where-Object {$_.Status -eq 'OK'}
```

Look for a port like COM3, COM4, COM5, etc. Use that number in the script.

## Troubleshooting

### "Python not found"
- Install Python from: https://www.python.org/downloads/
- **Important:** Check the box "Add Python to PATH" during installation

### "Module not found" or "No module named..."
- Make sure you activated the virtual environment (Step 2, command 2)
- You should see `(venv)` at the start of your prompt

### "Camera not found"
- Make sure your webcam is connected
- Close other programs using the camera (like Zoom, Skype, etc.)

### "Serial port error"
- The script will still work, it just won't send commands to ESP32
- Check if ESP32 is connected
- Make sure you updated the COM port in the script

## Quick Reference

**To start the hand tracker, always run these 3 commands:**

```powershell
cd "D:\Final Project\FinalRepo\final-project-gesture-car\mediapipe_hand_direction"
.\venv\Scripts\Activate.ps1
python hand_direction_tracker.py
```

That's it! You're ready to go.
