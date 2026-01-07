Import("env")

# Add motion_control.cpp and motor_driver.cpp from Vehicule to the build
import os
project_dir = env.get("PROJECT_DIR")
vehicule_control_dir = os.path.join(project_dir, "..", "Vehicule", "src", "control")
vehicule_drivers_dir = os.path.join(project_dir, "..", "Vehicule", "src", "drivers")

# Build the motion_control.cpp source file from Vehicule
env.BuildSources(
    os.path.join("$BUILD_DIR", "vehicule_control"),
    vehicule_control_dir,
    "motion_control.cpp"
)

# Build the motor_driver.cpp source file from Vehicule
env.BuildSources(
    os.path.join("$BUILD_DIR", "vehicule_drivers"),
    vehicule_drivers_dir,
    "motor_driver.cpp"
)

