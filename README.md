# Franka-Interface

> This branch implements working ROS2 Humble adoption for franka-interface with pixi packaging for maximal reproducibility and ease of installation.

This is a software package used for controlling and learning skills on the Franka Emika Panda Research Robot Arm.

Installation Instructions and Robot Setup Instructions are also available here: [https://iamlab-cmu.github.io/franka-interface](https://iamlab-cmu.github.io/franka-interface)

To join the Discord community, click the link [here](https://discord.gg/r6r7dttMwZ).

## Requirements

* Ubuntu 20.04 or higher
* [pixi](https://pixi.sh/latest/)
* (optional) [direnv](https://direnv.net/) to automatically source pixi shell

## Computer Setup Instructions

This library is intended to be installed on the computer that interfaces with the Franka (we call this the Control PC).
To use this library, refer to [FrankaPy](https://github.com/iamlab-cmu/frankapy), which can be run on any computer on the same ROS network and sends commands to `franka-interface`.

1. The Control PC should have an OS with real time kernel. The instructions for setting up a computer with the 18.04 Realtime Kernel from scratch are located here: [control pc ubuntu setup guide](old_docs/control_pc_ubuntu_setup_guide.md)
2. Instructions for setting up the computer specifically for Franka Robots is located here: [franka control pc setup guide](old_docs/franka_control_pc_setup_guide.md)

## Installation

1. Clone Repo and its Submodules:

   ```bash
   git clone https://github.com/iamlab-cmu/franka-interface.git   
   cd franka-interface
   git submodule update --init --recursive
   ```
All directories below are given relative to `/franka-interface`.

2. If you have direnv, allow it to automatically source the pixi shell:
   ```bash
   direnv allow
   ```
   Alternatively, source the shell via running:
   ```bash
   pixi shell
   ```
3. Build everything by running:
   ```bash
   pixi run build
   ```
   Under the hood, it does the following:
   1. Applies diff to the `libfranka`
   2. Builds `libfranka` and installs it in conda env
   3. Build `franka-interface` and installs it in conda env
   4. BUilds ROS2 workspace

4. Build franka-interface
   ```bash
   bash ./bash_scripts/make_franka_interface.sh
   ```
   Once it has finished building, you should see an application named `franka_interface` in the build folder.

5. Build ROS Node franka_ros_interface

   Make sure that you have installed ROS Kinetic / Melodic already and have added the `source /opt/ros/kinetic/setup.bash` or `source /opt/ros/melodic/setup.bash` into your `~/.bashrc` file. Make sure you have also installed catkin-tools either globally or in a virtual environment using the command `pip install catkin-tools`.

   ```bash
   bash ./bash_scripts/make_catkin.sh
   ```

6. To allow asynchronous gripper commands, we use the franka\_ros package, so install libfranka and franka\_ros using the following command (Change melodic to kinetic if you are on Ubuntu 16.04:
   ```bash
   sudo apt install ros-melodic-libfranka ros-melodic-franka-ros
   ```
   
## Citation

If this library proves useful to your research, please cite the paper below::
```
@article{zhang2020modular,
title={A modular robotic arm control stack for research: Franka-interface and frankapy},
author={Zhang, Kevin and Sharma, Mohit and Liang, Jacky and Kroemer, Oliver},
journal={arXiv preprint arXiv:2011.02398},
year={2020}
}
```

## Issues

#### LibPoco issue

libFranka requires libPoco, which can be installed using `sudo apt-get install libpoco-doc libpoco-dev`. However, trying to build libFranka might still fail since `CMAKE` cannot run ` find_package(Poco)` since there doesn't exist `/usr/local/lib/cmake/Poco/PocoConfig.cmake`. This is a peculiarity of libPoco which installs in a weird way without providing an option for us to link against it. 

To fix this we have copied the `libPoco.cmake` file in `{franka-interface-dir}/cmake`, and we add the following line to the CMakeLists.txt

`list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake`

Do the following if you run run into `libfranka: Cannot load model library: Cannot load library`:

```sh
mkdir -p /usr/local/lib/cmake/Poco/
cp cmake/FindPoco.cmake /usr/local/lib/cmake/FindPoco.cmake
```
