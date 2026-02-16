# Setup Instructions

Follow these steps to set up the project. Just copy and paste the commands.

## Step 1: Install Dependencies

> These instructions assume you downloaded or cloned the project to a folder. Replace the path with wherever you stored the repo.

### Linux/macOS (zsh/bash)

1. Open a terminal (zsh or bash)
2. Navigate to the project directory and run:

```bash
cd /path/to/final-project-gesture-car
./setup/install_dependencies_pipenv.sh
```

Or if you're already in the project root:

```bash
./setup/install_dependencies_pipenv.sh
```

**If you get a "permission denied" error, make the script executable first:**
```bash
chmod +x setup/install_dependencies_pipenv.sh
./setup/install_dependencies_pipenv.sh
```

### Windows

#### Option A: File Explorer (Easiest)

1. Open File Explorer and browse to your project folder  
   Example: `C:\Users\you\Documents\gesture-car`
2. Open the `setup` subfolder
3. Double-click `install_dependencies.bat`
4. Wait for the terminal window to finish (it can take a few minutes)

#### Option B: PowerShell (copy/paste friendly)

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

### Linux/macOS

1. Copy and paste the commands below, updating the path to match your machine:

```bash
cd /path/to/final-project-gesture-car/pc_side/Hand_Tracking
source venv/bin/activate
python Hand_Tracker.py
```

2. A camera window will open. Show your hand to the camera.
3. Press `q` to quit.

### Windows

1. Copy and paste the commands below, updating the path to match your machine:

```powershell
cd "C:\path\to\final-project-gesture-car\pc_side\Hand_Tracking"
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
2. Open `pc_side/Hand_Tracking/Hand_Tracker.py` or `pc_side/Hand_Tracking/constant.py` in a text editor
3. Update the serial port (see below how to find it)

### How to Find Your Serial Port

**Linux/macOS:**

Run this command in the terminal:

```bash
ls /dev/tty* | grep -E '(USB|ACM)'
```

Common ports are:
- Linux: `/dev/ttyUSB0`, `/dev/ttyACM0`
- macOS: `/dev/tty.usbserial-*`, `/dev/tty.usbmodem*`

Update the port in `pc_side/Hand_Tracking/Hand_Tracker.py` or `pc_side/Hand_Tracking/constant.py` (e.g., change `'COM11'` to `'/dev/ttyUSB0'`).

**Windows:**

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

**Linux/macOS - To start the hand tracker, always run these 3 commands (with your path):**

```bash
cd /path/to/final-project-gesture-car/pc_side/Hand_Tracking
source venv/bin/activate
python Hand_Tracker.py
```

**Windows - To start the hand tracker, always run these 3 commands (with your path):**

```powershell
cd "C:\path\to\final-project-gesture-car\pc_side\Hand_Tracking"
.\venv\Scripts\Activate.ps1
python Hand_Tracker.py
```

That's it! You're ready to go.
