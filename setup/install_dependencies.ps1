# Gesture Car Project - Dependency Installation Script
# This script sets up all Python dependencies needed for the project


# Helper function to find script if called with wrong path
function Find-ScriptPath {
    $currentDir = Get-Location
    $possiblePaths = @(
        $MyInvocation.MyCommand.Path
        $PSCommandPath
        (Join-Path $currentDir "setup\install_dependencies.ps1")
        (Join-Path $currentDir "..\setup\install_dependencies.ps1")
        (Join-Path (Split-Path $currentDir -Parent) "setup\install_dependencies.ps1")
    )
    
    foreach ($path in $possiblePaths) {
        if ($path -and (Test-Path $path)) {
            try {
                return Resolve-Path $path -ErrorAction Stop
            }
            catch {
                continue
            }
        }
    }
    return $null
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Gesture Car Project - Setup Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""


Write-Host "Checking Python installation..." -ForegroundColor Yellow

$pythonCmd = $null
$pythonVersion = $null

# Check for Python 3.10 using py launcher
$py310Check = Get-Command py -ErrorAction SilentlyContinue
if ($py310Check) {
    try {
        $py310Version = py -3.10 --version 2>&1
        if ($LASTEXITCODE -eq 0 -and $py310Version -match "Python 3\.10") {
            $pythonCmd = "py -3.10"
            $pythonVersion = $py310Version
            Write-Host "[OK] Found Python 3.10: $pythonVersion" -ForegroundColor Green
        }
    }
    catch {}
}

# If Python 3.10 not found, check for default python
if (-not $pythonCmd) {
    $pythonCheck = Get-Command python -ErrorAction SilentlyContinue
    if ($pythonCheck) {
        $pythonCmd = "python"
        $pythonVersion = python --version 2>&1
        Write-Host "[OK] Found: $pythonVersion" -ForegroundColor Green
    }
}

# If still not found, check py launcher for any Python version
if (-not $pythonCmd) {
    $pyCheck = Get-Command py -ErrorAction SilentlyContinue
    if ($pyCheck) {
        try {
            $pyVersion = py --version 2>&1
            if ($LASTEXITCODE -eq 0) {
                $pythonCmd = "py"
                $pythonVersion = $pyVersion
                Write-Host "[OK] Found: $pythonVersion" -ForegroundColor Green
            }
        }
        catch {}
    }
}

if (-not $pythonCmd) {
    Write-Host "[ERROR] Python is not installed or not in PATH!" -ForegroundColor Red
    Write-Host "Install Python 3.10 from: https://www.python.org/downloads/release/python-3100/" -ForegroundColor Yellow
    Write-Host "Make sure to check 'Add Python to PATH' during installation." -ForegroundColor Yellow
    exit 1
}

# Check Python version (need 3.7+, recommend 3.10 for mediapipe)
$versionPattern = 'Python\s+([0-9]+)\.([0-9]+)'
if ($pythonVersion -match $versionPattern) {
    $majorVersion = [int]$matches[1]
    $minorVersion = [int]$matches[2]
    if ($majorVersion -lt 3 -or ($majorVersion -eq 3 -and $minorVersion -lt 7)) {
        Write-Host "[ERROR] Python 3.7 or higher is required!" -ForegroundColor Red
        Write-Host "  Current version: Python $majorVersion.$minorVersion" -ForegroundColor Red
        exit 1
    }
    if ($majorVersion -gt 3 -or ($majorVersion -eq 3 -and $minorVersion -gt 12)) {
        Write-Host "[WARNING] Python version is over 3.12!" -ForegroundColor Yellow
        Write-Host "MediaPipe may not be compatible. Python 3.10 is recommended." -ForegroundColor Yellow
        Write-Host "Continue anyway? (y/N): " -ForegroundColor Yellow -NoNewline
        $continue = Read-Host
        if ($continue -ne "y" -and $continue -ne "Y") {
            Write-Host "Installation cancelled." -ForegroundColor Red
            exit 1
        }
    }
}

$scriptFullPath = Find-ScriptPath
if (-not $scriptFullPath) {
    Write-Host "[ERROR] Could not locate the script!" -ForegroundColor Red
    Write-Host "Run from project root: .\setup\install_dependencies.ps1" -ForegroundColor Yellow
    exit 1
}

$scriptPath = Split-Path -Parent $scriptFullPath
$projectRoot = Split-Path -Parent $scriptPath
$handTrackingDir = Join-Path $projectRoot "pc_side\Hand_Tracking"

if (-not (Test-Path $handTrackingDir)) {
    Write-Host "[ERROR] Directory 'pc_side\Hand_Tracking' not found!" -ForegroundColor Red
    exit 1
}

Set-Location $projectRoot

$venvPath = Join-Path $handTrackingDir "venv"
if (Test-Path $venvPath) {
    Write-Host "Virtual environment already exists." -ForegroundColor Yellow
    Write-Host "Recreate it? (y/N): " -ForegroundColor Yellow -NoNewline
    $recreate = Read-Host
    if ($recreate -eq "y" -or $recreate -eq "Y") {
        Remove-Item -Recurse -Force $venvPath
        Write-Host "[OK] Removed old virtual environment" -ForegroundColor Green
    }
}

if (-not (Test-Path $venvPath)) {
    Write-Host "Creating virtual environment..." -ForegroundColor Yellow
    Invoke-Expression "$pythonCmd -m venv venv"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Failed to create virtual environment!" -ForegroundColor Red
        exit 1
    }
    Write-Host "[OK] Virtual environment created" -ForegroundColor Green
}

Write-Host "Activating virtual environment..." -ForegroundColor Yellow
$activateScript = Join-Path $venvPath "Scripts\Activate.ps1"
if (Test-Path $activateScript) {
    & $activateScript
    Write-Host "[OK] Virtual environment activated" -ForegroundColor Green
}
else {
    Write-Host "[ERROR] Failed to activate virtual environment!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Upgrading pip..." -ForegroundColor Yellow
Invoke-Expression "$pythonCmd -m pip install --upgrade pip --quiet"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Failed to upgrade pip!" -ForegroundColor Red
    exit 1
}
Write-Host "[OK] pip upgraded" -ForegroundColor Green

Write-Host ""
Write-Host "Installing Python dependencies..." -ForegroundColor Yellow
$requirementsFile = Join-Path $projectRoot "requirements.txt"
if (Test-Path $requirementsFile) {
    $venvPip = Join-Path $venvPath "Scripts\pip.exe"
    if (Test-Path $venvPip) {
        & $venvPip install -r $requirementsFile
    }
    else {
        Invoke-Expression "$pythonCmd -m pip install -r $requirementsFile"
    }
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Failed to install dependencies!" -ForegroundColor Red
        exit 1
    }
    Write-Host "[OK] All Python dependencies installed successfully" -ForegroundColor Green
}
else {
    Write-Host "[ERROR] requirements.txt not found in project root!" -ForegroundColor Red
    Write-Host "Expected location: $requirementsFile" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Setup Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "To use the project:" -ForegroundColor Yellow
Write-Host "  1. Activate: cd pc_side\Hand_Tracking && .\venv\Scripts\Activate.ps1" -ForegroundColor White
Write-Host "  2. Run: python Hand_Tracker.py" -ForegroundColor White
Write-Host "  3. Update COM port in pc_side\Hand_Tracking\constant.py if needed" -ForegroundColor White
Write-Host ""
