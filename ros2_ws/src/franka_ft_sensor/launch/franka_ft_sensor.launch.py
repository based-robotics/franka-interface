from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('ft_ip', default_value='172.16.0.12'),
        DeclareLaunchArgument('pub_rate_hz', default_value='976.0'),
        DeclareLaunchArgument('publish_wrench', default_value='false'),
        DeclareLaunchArgument('frame_id', default_value='base_link'),
        DeclareLaunchArgument('diag_publish_period_s', default_value='0.01'),

        Node(
            package='franka_ft_sensor',
            executable='netft_node',
            name='netft_node',
            output='screen',
            parameters=[{
                'address': LaunchConfiguration('ft_ip'),
                'pub_rate_hz': LaunchConfiguration('pub_rate_hz'),
                'publish_wrench': LaunchConfiguration('publish_wrench'),
                'frame_id': LaunchConfiguration('frame_id'),
                'diag_publish_period_s': LaunchConfiguration('diag_publish_period_s')
            }]
        )
    ])
