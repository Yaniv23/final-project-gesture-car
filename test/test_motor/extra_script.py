Import("env")

# Add motion_control.cpp and motor_driver.cpp from Car to the build
import os
project_dir = env.get("PROJECT_DIR")
car_control_dir = os.path.join(project_dir, "..", "Car", "src", "control")
car_drivers_dir = os.path.join(project_dir, "..", "Car", "src", "drivers")

# Build the motion_control.cpp source file from Car
env.BuildSources(
    os.path.join("$BUILD_DIR", "car_control"),
    car_control_dir,
    "motion_control.cpp"
)

# Build the motor_driver.cpp source file from Car
env.BuildSources(
    os.path.join("$BUILD_DIR", "car_drivers"),
    car_drivers_dir,
    "motor_driver.cpp"
)

