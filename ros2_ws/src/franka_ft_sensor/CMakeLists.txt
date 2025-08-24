cmake_minimum_required(VERSION 3.8)
project(franka_ft_sensor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(diagnostic_updater REQUIRED)
find_package(std_msgs REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)

include_directories(
  include
)

add_library(netft_rdt_driver
  src/netft_rdt_driver.cpp
)
target_link_libraries(netft_rdt_driver
  Boost::system
  Boost::thread
)
ament_target_dependencies(netft_rdt_driver
  rclcpp geometry_msgs diagnostic_msgs diagnostic_updater std_msgs
)

add_executable(netft_node
  src/netft_node.cpp
)
target_link_libraries(netft_node
  netft_rdt_driver
)
ament_target_dependencies(netft_node
  rclcpp geometry_msgs diagnostic_msgs diagnostic_updater std_msgs
)

install(TARGETS
  netft_rdt_driver
  netft_node
  RUNTIME DESTINATION lib/${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
)

install(DIRECTORY include/
  DESTINATION include
)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
