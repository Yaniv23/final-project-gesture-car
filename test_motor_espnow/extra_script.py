Import("env")

# Add necessary source files from Vehicule to the build
# Using BuildSources to add files before main program construction
import os
project_dir = env.get("PROJECT_DIR")
vehicule_src_dir = os.path.join(project_dir, "..", "Vehicule", "src")

# Print for debugging
print("Building Vehicule sources from:", vehicule_src_dir)

# Build motion_control.cpp
vehicule_control_dir = os.path.join(vehicule_src_dir, "control")
env.BuildSources(
    os.path.join("$BUILD_DIR", "vehicule_control"),
    vehicule_control_dir,
    "motion_control.cpp"
)

# Build motor_driver.cpp
vehicule_drivers_dir = os.path.join(vehicule_src_dir, "drivers")
env.BuildSources(
    os.path.join("$BUILD_DIR", "vehicule_drivers"),
    vehicule_drivers_dir,
    "motor_driver.cpp"
)

# Build communication files
vehicule_comm_dir = os.path.join(vehicule_src_dir, "communication")
env.BuildSources(
    os.path.join("$BUILD_DIR", "vehicule_communication"),
    vehicule_comm_dir,
    "espnow_handler.cpp"
)
env.BuildSources(
    os.path.join("$BUILD_DIR", "vehicule_communication"),
    vehicule_comm_dir,
    "command_protocol.cpp"
)

# Build shared files
vehicule_shared_dir = os.path.join(vehicule_src_dir, "shared")
env.BuildSources(
    os.path.join("$BUILD_DIR", "vehicule_shared"),
    vehicule_shared_dir,
    "queues.cpp"
)

# Build safety files (timeout monitor, emergency stop, watchdog)
vehicule_safety_dir = os.path.join(vehicule_src_dir, "safety")
env.BuildSources(
    os.path.join("$BUILD_DIR", "vehicule_safety"),
    vehicule_safety_dir,
    "timeout_monitor.cpp"
)
env.BuildSources(
    os.path.join("$BUILD_DIR", "vehicule_safety"),
    vehicule_safety_dir,
    "emergency_stop.cpp"
)
env.BuildSources(
    os.path.join("$BUILD_DIR", "vehicule_safety"),
    vehicule_safety_dir,
    "watchdog.cpp"
)

