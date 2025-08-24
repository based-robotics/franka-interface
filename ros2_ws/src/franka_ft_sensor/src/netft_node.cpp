/*********************************************************************
 * ROS 2 Humble node: NetFT → ROS2
 *********************************************************************/
#include <rclcpp/rclcpp.hpp>

#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <diagnostic_updater/diagnostic_status_wrapper.hpp>
#include <geometry_msgs/msg/wrench_stamped.hpp>
#include <geometry_msgs/msg/wrench.hpp>
#include <std_msgs/msg/bool.hpp>

#include <memory>
#include <thread>

#include "franka_ft_sensor/netft_rdt_driver.h"

class NetFTNode : public rclcpp::Node {
public:
  NetFTNode() : Node("netft_node") {
    // Parameters (same defaults as ROS1)
    address_         = this->declare_parameter<std::string>("address", "172.16.0.12");
    pub_rate_hz_     = this->declare_parameter<double>("pub_rate_hz", 976.0);
    publish_wrench_  = this->declare_parameter<bool>("publish_wrench", false);
    frame_id_        = this->declare_parameter<std::string>("frame_id", "base_link");
    diag_period_     = this->declare_parameter<double>("diag_publish_period_s", 0.01);

    // Publishers
    ready_pub_ = this->create_publisher<std_msgs::msg::Bool>("netft_ready", 1);
    if (publish_wrench_) {
      wrench_pub_ = this->create_publisher<geometry_msgs::msg::Wrench>("netft_data", 10);
    } else {
      wrench_stamped_pub_ = this->create_publisher<geometry_msgs::msg::WrenchStamped>("netft_data", 10);
    }
    diag_pub_ = this->create_publisher<diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 2);

    // Driver
    std_msgs::msg::Bool ready;
    try {
      driver_ = std::make_shared<netft_rdt_driver::NetFTRDTDriver>(address_);
      ready.data = true;
    } catch (const std::exception &e) {
      RCLCPP_ERROR(this->get_logger(), "NetFT driver init failed: %s", e.what());
      ready.data = false;
    }
    ready_pub_->publish(ready);

    // Start publish loop thread
    running_ = true;
    pub_thread_ = std::thread([this]() { this->publishLoop(); });

    // Diagnostics timer
    diag_timer_ = this->create_wall_timer(
      std::chrono::duration<double>(diag_period_),
      [this]() { this->publishDiagnostics(); }
    );
  }

  ~NetFTNode() override {
    running_ = false;
    if (pub_thread_.joinable()) pub_thread_.join();
  }

private:
  void publishLoop() {
    const auto period = std::chrono::duration<double>(1.0 / std::max(1.0, pub_rate_hz_));
    geometry_msgs::msg::WrenchStamped data;

    while (rclcpp::ok() && running_) {
      auto t_start = std::chrono::steady_clock::now();

      if (driver_ && driver_->waitForNewData()) {
        driver_->getData(data);
        data.header.stamp = this->now();
        data.header.frame_id = frame_id_;

        if (publish_wrench_) {
          geometry_msgs::msg::Wrench w = data.wrench;
          wrench_pub_->publish(w);
        } else {
          wrench_stamped_pub_->publish(data);
        }
      }

      // keep loop rate close to pub_rate_hz_
      auto elapsed = std::chrono::steady_clock::now() - t_start;
      auto sleep_remaining = period - elapsed;
      if (sleep_remaining > std::chrono::duration<double>(0)) {
        std::this_thread::sleep_for(sleep_remaining);
      } else {
        // overrun; continue
      }
    }
  }

  void publishDiagnostics() {
    if (!driver_) return;

    diagnostic_updater::DiagnosticStatusWrapper wrap;
    driver_->diagnostics(wrap);

    diagnostic_msgs::msg::DiagnosticArray arr;
    arr.header.stamp = this->now();
    arr.status.push_back(static_cast<diagnostic_msgs::msg::DiagnosticStatus>(wrap));
    diag_pub_->publish(arr);

    // keep ready heartbeat flowing
    std_msgs::msg::Bool ready;
    ready.data = true;
    ready_pub_->publish(ready);
  }

private:
    // params
    std::string address_;
    double pub_rate_hz_{976.0};
    bool publish_wrench_{false};
    std::string frame_id_;
    double diag_period_{0.01};

    // pubs
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr ready_pub_;
    rclcpp::Publisher<geometry_msgs::msg::WrenchStamped>::SharedPtr wrench_stamped_pub_;
    rclcpp::Publisher<geometry_msgs::msg::Wrench>::SharedPtr        wrench_pub_;
    rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diag_pub_;

    // driver, timers, threads
    std::shared_ptr<netft_rdt_driver::NetFTRDTDriver> driver_;
    rclcpp::TimerBase::SharedPtr diag_timer_;
    std::thread pub_thread_;
    std::atomic<bool> running_{false};
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<NetFTNode>());
  rclcpp::shutdown();
  return 0;
}
