# Setup Instructions

Follow these steps to set up the project. Just copy and paste the commands.

## Step 1: Install Dependencies

> These instructions assume you downloaded or cloned the project to a folder such as `C:\Users\you\Documents\gesture-car`. Replace that path with wherever you stored the repo.

### Option A: File Explorer (Easiest)

1. Open File Explorer and browse to your project folder  
   Example: `C:\Users\you\Documents\gesture-car`
2. Open the `setup` subfolder
3. Double-click `install_dependencies.bat`
4. Wait for the terminal window to finish (it can take a few minutes)

### Option B: PowerShell (copy/paste friendly)

1. Open PowerShell
2. Run the command below, replacing the path with the folder where the repo lives:

```powershell
cd "C:\path\to\final-project-gesture-car"
.\setup\install_dependencies.ps1
```

**If you get an execution-policy error, run this once and try again:**
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

## Step 2: Run the Hand Tracker

1. Copy and paste the commands below, updating the path to match your machine:

```powershell
cd "C:\path\to\final-project-gesture-car\Hand_Tracking"
```

```powershell
.\venv\Scripts\Activate.ps1
```

```powershell
python Hand_Tracker.py
```

2. A camera window will open. Show your hand to the camera.
3. Press `q` to quit.

## Step 3: Connect ESP32 (Optional)

If you want to control the car:

1. Connect your ESP32 to the computer via USB
2. Open `Hand_Tracker.py` in a text editor
3. Find the line that says: `COM_PORT = 'COM11'` (or check `constant.py`)
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

**To start the hand tracker, always run these 3 commands (with your path):**

```powershell
cd "C:\path\to\final-project-gesture-car\Hand_Tracking"
.\venv\Scripts\Activate.ps1
python Hand_Tracker.py
```

That's it! You're ready to go.
