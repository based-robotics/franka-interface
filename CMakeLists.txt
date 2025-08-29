cmake_minimum_required (VERSION 3.4)
project (franka-interface
  VERSION 0.0.1 
  LANGUAGES CXX
)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra)

find_package(Eigen3 REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)

include(GNUInstallDirs)
include_directories(${Boost_INCLUDE_DIRS})

add_subdirectory(franka-interface)
add_subdirectory(franka-interface-common)
# add_subdirectory(libfranka)

add_executable(franka_interface src/franka_interface.cpp)

target_link_libraries(franka_interface 
  franka-interface 
  franka-interface-common
  franka
  Eigen3::Eigen
  ${Boost_LIBRARIES}
)

# install(TARGETS franka_interface DESTINATION bin)

include(GNUInstallDirs)

# Force-install all libraries from the top-level (robust to subproject configs)
install(TARGETS
  franka-interface
  franka-interface-common
  proto
  EXPORT franka_interface_targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
