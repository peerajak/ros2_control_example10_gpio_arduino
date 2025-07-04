cmake_minimum_required(VERSION 3.16)
project(ros2_control_demo_example_10_myhardware LANGUAGES CXX)

if(CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# set the same behavior for windows as it is on linux
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# find dependencies
set(THIS_PACKAGE_INCLUDE_DEPENDS
  hardware_interface
  control_msgs
  std_msgs
  pluginlib
  rclcpp
  rclcpp_lifecycle
  "${SERIAL_INCLUDE_DIRS}"
)

# find dependencies
find_package(backward_ros REQUIRED)
find_package(ament_cmake REQUIRED)
find_package(PkgConfig)
pkg_check_modules(SERIAL libserial)
foreach(Dependency IN ITEMS ${THIS_PACKAGE_INCLUDE_DEPENDS})
  find_package(${Dependency} REQUIRED)
endforeach()

## COMPILE

message(SERIAL_INCLUDE_DIRS="${SERIAL_INCLUDE_DIRS}")
message(SERIAL_LDFLAGS="${SERIAL_LDFLAGS}")


add_library(
  ros2_control_demo_example_10_myhardware
  SHARED
  hardware/rrbot.cpp
)
target_compile_features(ros2_control_demo_example_10_myhardware PUBLIC cxx_std_17)
target_include_directories(ros2_control_demo_example_10_myhardware PUBLIC
"${SERIAL_INCLUDE_DIRS}"
$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/hardware/include>
$<INSTALL_INTERFACE:include/ros2_control_demo_example_10_myhardware>
)
target_link_libraries(ros2_control_demo_example_10_myhardware PRIVATE
 "${SERIAL_LDFLAGS}")

ament_target_dependencies(
  ros2_control_demo_example_10_myhardware PUBLIC
  ${THIS_PACKAGE_INCLUDE_DEPENDS}
)

# Export hardware plugins
pluginlib_export_plugin_description_file(hardware_interface ros2_control_demo_example_10_myhardware.xml)

# INSTALL
install(
  DIRECTORY hardware/include/
  DESTINATION include/ros2_control_demo_example_10_myhardware
)
install(
  DIRECTORY description/launch description/ros2_control description/urdf
  DESTINATION share/ros2_control_demo_example_10_myhardware
)
install(
  DIRECTORY bringup/launch bringup/config
  DESTINATION share/ros2_control_demo_example_10_myhardware
)
install(TARGETS ros2_control_demo_example_10_myhardware
  EXPORT export_ros2_control_demo_example_10_myhardware
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

if(BUILD_TESTING)
  find_package(ament_cmake_pytest REQUIRED)

  ament_add_pytest_test(example_10_urdf_xacro test/test_urdf_xacro.py)
  ament_add_pytest_test(view_example_10_launch test/test_view_robot_launch.py)
  ament_add_pytest_test(run_example_10_launch test/test_rrbot_launch.py)
endif()

## EXPORTS
ament_export_targets(export_ros2_control_demo_example_10_myhardware HAS_LIBRARY_TARGET)
ament_export_dependencies(${THIS_PACKAGE_INCLUDE_DEPENDS})
ament_package()
