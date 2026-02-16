Import("env")

"""
Auto-include script for Car sources in test_motor_espnow.
This script automatically scans directories and adds all required .cpp files
without needing to list them manually.

IMPORTANT: This script is the ONLY mechanism for adding files from Car/src.
"""

import os
import glob

# Configuration: files to EXCLUDE (not used in this test)
EXCLUDED_FILES = [
    "servo_driver.cpp",        # Requires ESP32Servo library
    "ultrasonic_driver.cpp",   # Not used in this test
    "task_sensor_fusion.cpp",  # Uses ultrasonic sensors
    "task_telemetry.cpp",      # Optional for this test
    "main.cpp",                # We use local main.cpp
]

# Directories to include from Car/src
INCLUDED_DIRS = [
    "control",
    "drivers",
    "communication",
    "shared",
    "safety",
]

project_dir = env.get("PROJECT_DIR")
car_src_dir = os.path.join(project_dir, "..", "Car", "src")

# Check that Car/src directory exists
if not os.path.exists(car_src_dir):
    print(f"[ERROR] Car/src directory not found: {car_src_dir}")
    print("[ERROR] Ensure the Car project exists.")
    exit(1)

# Ensure external files have access to Arduino framework headers
# Files compiled via BuildSources should inherit the same includes as src/
framework_path = env.get("FRAMEWORK_DIR")
if framework_path:
    wifi_lib_path = os.path.join(framework_path, "libraries", "WiFi", "src")
    if os.path.exists(wifi_lib_path):
        env.Append(CPPPATH=[wifi_lib_path])
        print(f"[INFO] WiFi library path added: {wifi_lib_path}")

print("=" * 60)
print("Auto-including files from Car/src")
print("Source directory:", car_src_dir)
print("=" * 60)

# Count files added and excluded
files_added = 0
files_excluded = 0

# Iterate each directory and add .cpp files
for dir_name in INCLUDED_DIRS:
    dir_path = os.path.join(car_src_dir, dir_name)

    if not os.path.exists(dir_path):
        print(f"[WARNING] Directory not found: {dir_path}")
        continue

    # Find all .cpp files in this directory
    cpp_files = glob.glob(os.path.join(dir_path, "*.cpp"))

    if not cpp_files:
        print(f"[INFO] No .cpp files in {dir_name}/")
        continue

    # Build src_filter to exclude unwanted files
    src_filter_parts = ["+<*.cpp>"]
    for excluded_file in EXCLUDED_FILES:
        src_filter_parts.append(f"-<{excluded_file}>")
    src_filter = " ".join(src_filter_parts)

    # List files that will be included/excluded for the log
    for cpp_file in sorted(cpp_files):
        filename = os.path.basename(cpp_file)
        if filename in EXCLUDED_FILES:
            print(f"[SKIP] Excluded: {dir_name}/{filename}")
            files_excluded += 1
        elif os.path.isfile(cpp_file):
            print(f"[ADD] {dir_name}/{filename}")
            files_added += 1

    # Add directory with BuildSources and filter
    build_dir = os.path.join("$BUILD_DIR", f"car_{dir_name}")
    env.BuildSources(build_dir, dir_path, src_filter)

print("=" * 60)
print("Summary:")
print(f"  - Files added: {files_added}")
print(f"  - Files excluded: {files_excluded}")
print("=" * 60)
