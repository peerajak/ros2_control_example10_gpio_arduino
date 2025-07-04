// Copyright 2023 ros2_control Development Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ros2_control_demo_example_10_myhardware/rrbot.hpp"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <vector>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"

#define DO_WRITING
#define DO_READING

namespace ros2_control_demo_example_10_myhardware
{
  RRBotSystemWithGPIOHardware::RRBotSystemWithGPIOHardware()
  {
  }
  
  
  RRBotSystemWithGPIOHardware::~RRBotSystemWithGPIOHardware()
  {
    if (arduino_.IsOpen())
    {
      try
      {
        arduino_.Close();
      }
      catch (...)
      {
        RCLCPP_FATAL_STREAM(rclcpp::get_logger("RRBotSystemWithGPIOHardware"),
                            "Something went wrong while closing connection with port " << port_);
      }
    }
  }
  
hardware_interface::CallbackReturn RRBotSystemWithGPIOHardware::on_init(
  const hardware_interface::HardwareInfo & info)
{
  if (
    hardware_interface::SystemInterface::on_init(info) !=
    hardware_interface::CallbackReturn::SUCCESS)
  {
    return hardware_interface::CallbackReturn::ERROR;
  }
  logger_ = std::make_shared<rclcpp::Logger>(
    rclcpp::get_logger("controller_manager.resource_manager.hardware_component.system.RRBot"));
  clock_ = std::make_shared<rclcpp::Clock>(rclcpp::Clock());

  hw_states_.resize(info_.joints.size(), std::numeric_limits<double>::quiet_NaN());
  hw_commands_.resize(info_.joints.size(), std::numeric_limits<double>::quiet_NaN());


  try
  {
    port_ = info_.hardware_parameters.at("port");
  }
  catch (const std::out_of_range &e)
  {
    RCLCPP_FATAL(rclcpp::get_logger("RRBotSystemWithGPIOHardware"), "No Serial Port provided! Aborting");
    return CallbackReturn::FAILURE;
  }


  for (const hardware_interface::ComponentInfo & joint : info_.joints)
  {
    // RRBotSystemPositionOnly has exactly one state and command interface on each joint
    if (joint.command_interfaces.size() != 1)
    {
      RCLCPP_FATAL(
        get_logger(), "Joint '%s' has %zu command interfaces found. 1 expected.",
        joint.name.c_str(), joint.command_interfaces.size());
      return hardware_interface::CallbackReturn::ERROR;
    }

    if (joint.command_interfaces[0].name != hardware_interface::HW_IF_POSITION)
    {
      RCLCPP_FATAL(
        get_logger(), "Joint '%s' have %s command interfaces found. '%s' expected.",
        joint.name.c_str(), joint.command_interfaces[0].name.c_str(),
        hardware_interface::HW_IF_POSITION);
      return hardware_interface::CallbackReturn::ERROR;
    }

    if (joint.state_interfaces.size() != 1)
    {
      RCLCPP_FATAL(
        get_logger(), "Joint '%s' has %zu state interface. 1 expected.", joint.name.c_str(),
        joint.state_interfaces.size());
      return hardware_interface::CallbackReturn::ERROR;
    }

    if (joint.state_interfaces[0].name != hardware_interface::HW_IF_POSITION)
    {
      RCLCPP_FATAL(
        get_logger(), "Joint '%s' have %s state interface. '%s' expected.", joint.name.c_str(),
        joint.state_interfaces[0].name.c_str(), hardware_interface::HW_IF_POSITION);
      return hardware_interface::CallbackReturn::ERROR;
    }
  }

  // RRBotSystemWithGPIOHardware has exactly two GPIO components
  if (info_.gpios.size() != 2)
  {
    RCLCPP_FATAL(
      get_logger(), "RRBotSystemWithGPIOHardware has '%ld' GPIO components, '%d' expected.",
      info_.gpios.size(), 2);
    return hardware_interface::CallbackReturn::ERROR;
  }
  // with exactly 1 command interface
  for (int i = 0; i < 2; i++)
  {
    if (info_.gpios[i].command_interfaces.size() != 1)
    {
      RCLCPP_FATAL(
        get_logger(), "GPIO component %s has '%ld' command interfaces, '%d' expected.",
        info_.gpios[i].name.c_str(), info_.gpios[i].command_interfaces.size(), 1);
      return hardware_interface::CallbackReturn::ERROR;
    }
  }
  // and 3/1 state interfaces, respectively
  if (info_.gpios[0].state_interfaces.size() != 3)
  {
    RCLCPP_FATAL(
      get_logger(), "GPIO component %s has '%ld' state interfaces, '%d' expected.",
      info_.gpios[0].name.c_str(), info_.gpios[0].state_interfaces.size(), 3);
    return hardware_interface::CallbackReturn::ERROR;
  }
  if (info_.gpios[1].state_interfaces.size() != 1)
  {
    RCLCPP_FATAL(
      get_logger(), "GPIO component %s has '%ld' state interfaces, '%d' expected.",
      info_.gpios[1].name.c_str(), info_.gpios[1].state_interfaces.size(), 1);
    return hardware_interface::CallbackReturn::ERROR;
  }

  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn RRBotSystemWithGPIOHardware::on_configure(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  // BEGIN: This part here is for exemplary purposes - Please do not copy to your production code
  RCLCPP_INFO(get_logger(), "Configuring ...please wait...");
  // END: This part here is for exemplary purposes - Please do not copy to your production code

  // reset values always when configuring hardware
  std::fill(hw_states_.begin(), hw_states_.end(), 0);
  std::fill(hw_commands_.begin(), hw_commands_.end(), 0);
  std::fill(hw_gpio_in_.begin(), hw_gpio_in_.end(), 0);
  std::fill(hw_gpio_out_.begin(), hw_gpio_out_.end(), 0);

  RCLCPP_INFO(get_logger(), "Successfully configured!");

  return hardware_interface::CallbackReturn::SUCCESS;
}

std::vector<hardware_interface::StateInterface>
RRBotSystemWithGPIOHardware::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> state_interfaces;
  for (uint i = 0; i < info_.joints.size(); i++)
  {
    state_interfaces.emplace_back(
      hardware_interface::StateInterface(
        info_.joints[i].name, hardware_interface::HW_IF_POSITION, &hw_states_[i]));
  }

  RCLCPP_INFO(get_logger(), "State interfaces:");
  hw_gpio_in_.resize(4);
  size_t ct = 0;
  for (size_t i = 0; i < info_.gpios.size(); i++)
  {
    for (auto state_if : info_.gpios.at(i).state_interfaces)
    {
      state_interfaces.emplace_back(
        hardware_interface::StateInterface(
          info_.gpios.at(i).name, state_if.name, &hw_gpio_in_[ct++]));
      RCLCPP_INFO(
        get_logger(), "Added %s/%s", info_.gpios.at(i).name.c_str(), state_if.name.c_str());
    }
  }

  return state_interfaces;
}

std::vector<hardware_interface::CommandInterface>
RRBotSystemWithGPIOHardware::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> command_interfaces;
  for (uint i = 0; i < info_.joints.size(); i++)
  {
    command_interfaces.emplace_back(
      hardware_interface::CommandInterface(
        info_.joints[i].name, hardware_interface::HW_IF_POSITION, &hw_commands_[i]));
  }
  RCLCPP_INFO(get_logger(), "Command interfaces:");
  hw_gpio_out_.resize(2);
  size_t ct = 0;
  for (size_t i = 0; i < info_.gpios.size(); i++)
  {
    for (auto command_if : info_.gpios.at(i).command_interfaces)
    {
      command_interfaces.emplace_back(
        hardware_interface::CommandInterface(
          info_.gpios.at(i).name, command_if.name, &hw_gpio_out_[ct++]));
      RCLCPP_INFO(
        get_logger(), "Added %s/%s", info_.gpios.at(i).name.c_str(), command_if.name.c_str());
    }
  }

  return command_interfaces;
}

hardware_interface::CallbackReturn RRBotSystemWithGPIOHardware::on_activate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  // BEGIN: This part here is for exemplary purposes - Please do not copy to your production code
  RCLCPP_INFO(get_logger(), "Activating ...please wait...");
  // END: This part here is for exemplary purposes - Please do not copy to your production code

  // command and state should be equal when starting
  for (uint i = 0; i < hw_states_.size(); i++)
  {
    hw_commands_[i] = hw_states_[i];
  }
  RCLCPP_INFO(get_logger(), "Activating Arduino...please wait...");
  try
  {
    arduino_.Open(port_);
    arduino_.SetBaudRate(LibSerial::BaudRate::BAUD_115200);
  }
  catch (...)
  {
    RCLCPP_FATAL_STREAM(rclcpp::get_logger("RRBotSystemWithGPIOHardware"),
                        "Something went wrong while interacting with port " << port_);
    return CallbackReturn::FAILURE;
  }
  RCLCPP_INFO(get_logger(), "Successfully activated!");

  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn RRBotSystemWithGPIOHardware::on_deactivate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  // BEGIN: This part here is for exemplary purposes - Please do not copy to your production code
  RCLCPP_INFO(get_logger(), "Successfully deactivated!");
  // END: This part here is for exemplary purposes - Please do not copy to your production code
  if (arduino_.IsOpen())
  {
    try
    {
      arduino_.Close();
    }
    catch (...)
    {
      RCLCPP_FATAL_STREAM(rclcpp::get_logger("RRBotSystemWithGPIOHardware"),
                          "Something went wrong while closing connection with port " << port_);
    }
  }

  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::return_type RRBotSystemWithGPIOHardware::read(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  // BEGIN: This part here is for exemplary purposes - Please do not copy to your production code

  std::stringstream ss;
  ss << "Reading states:";

  for (uint i = 0; i < hw_states_.size(); i++)
  {
    // Simulate RRBot's movement
    hw_states_[i] = hw_states_[i] + (hw_commands_[i] - hw_states_[i]);
  }


  // mirror GPIOs back
  hw_gpio_in_[0] = hw_gpio_out_[0];
  hw_gpio_in_[3] = hw_gpio_out_[1];
  // random inputs
  unsigned int seed = time(NULL) + 1;

  seed = time(NULL) + 2;
  hw_gpio_in_[1] = static_cast<float>(rand_r(&seed));
  hw_gpio_in_[2] = static_cast<float>(rand_r(&seed));

  for (uint i = 0; i < hw_gpio_in_.size(); i++)
  {
    ss << std::fixed << std::setprecision(2) << std::endl
       << "\t" << hw_gpio_in_[i] << " from GPIO input '" << static_cast<int>(i) << "'";
  }
  RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 500, "%s", ss.str().c_str());
  // END: This part here is for exemplary purposes - Please do not copy to your production code
#ifdef DO_READING
  std::string message;
  if(arduino_.IsDataAvailable()){ 
    arduino_.ReadLine(message);
    RCLCPP_INFO(rclcpp::get_logger("RRBotSystemWithGPIOHardware_read"), message.c_str());
  }else{
    RCLCPP_INFO(rclcpp::get_logger("RRBotSystemWithGPIOHardware_read"),"Error arduino not available");
  }
  // try{
  //   hw_gpio_in_[1] =  std::stof(message); 
  // } catch (const std::invalid_argument& e) {
  //   std::cerr << "Error converting '" << message << "': Invalid argument - " << e.what() << std::endl;
  //   hw_gpio_in_[1] = 0.0;
  // } catch (const std::out_of_range& e) {
  //   std::cerr << "Error converting '" << message << "': Out of range - " << e.what() << std::endl;
  //   hw_gpio_in_[1] = 0.0;
  // }
#endif
  return hardware_interface::return_type::OK;
}

hardware_interface::return_type RRBotSystemWithGPIOHardware::write(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  // BEGIN: This part here is for exemplary purposes - Please do not copy to your production code
  std::stringstream ss;
  ss << "Writing commands:";

  for (uint i = 0; i < hw_gpio_out_.size(); i++)
  {
    ss << std::fixed << std::setprecision(2) << std::endl
       << "\t" << hw_gpio_out_[i] << " for GPIO output '" << static_cast<int>(i) << "'";
  }
  RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 500, "%s", ss.str().c_str());
  // END: This part here is for exemplary purposes - Please do not copy to your production code
#ifdef DO_WRITING
  if(arduino_.IsDataAvailable()){ 
    try{
      std::string is_zero("0");
      std::string is_one("1");
      //FIXME Arduino Write something
      if(hw_gpio_out_[1] == 0.27){
        arduino_.Write(is_one);
        RCLCPP_INFO(rclcpp::get_logger("RRBotSystemWithGPIOHardware"),"write_one");
      }else{
      arduino_.Write(is_zero);
      RCLCPP_INFO(rclcpp::get_logger("RRBotSystemWithGPIOHardware"),"write_zero");
      }
    }
    catch (...) {
      RCLCPP_ERROR_STREAM(rclcpp::get_logger("RRBotSystemWithGPIOHardware"),
                          "Something went wrong while sending the message to the port " << port_);
      return hardware_interface::return_type::ERROR;
    }
  }else{
    RCLCPP_INFO(rclcpp::get_logger("RRBotSystemWithGPIOHardware_write"),"Error arduino not available");
  } 
#endif
  return hardware_interface::return_type::OK;
}

}  // namespace ros2_control_demo_example_10_myhardware

#include "pluginlib/class_list_macros.hpp"

PLUGINLIB_EXPORT_CLASS(
  ros2_control_demo_example_10_myhardware::RRBotSystemWithGPIOHardware, hardware_interface::SystemInterface)
