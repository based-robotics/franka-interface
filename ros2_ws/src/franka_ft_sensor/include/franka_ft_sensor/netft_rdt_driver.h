#pragma once
/*********************************************************************
 * Port of NetFT RDT driver header to ROS 2 Humble
 *********************************************************************/

#include <boost/asio.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#include <string>
#include <chrono>

#include <diagnostic_updater/diagnostic_status_wrapper.hpp>
#include <geometry_msgs/msg/wrench_stamped.hpp>

namespace netft_rdt_driver {

class NetFTRDTDriver {
public:
  explicit NetFTRDTDriver(const std::string &address);
  ~NetFTRDTDriver();

  // Get newest RDT data
  void getData(geometry_msgs::msg::WrenchStamped &data);

  // Fill diagnostics wrapper
  void diagnostics(diagnostic_updater::DiagnosticStatusWrapper &d);

  // Wait (up to ~100ms) for new data
  bool waitForNewData();

private:
  void recvThreadFunc();
  void startStreaming();

  enum { RDT_PORT = 49152 };
  std::string address_;

  boost::asio::io_service io_service_;
  boost::asio::ip::udp::socket socket_;

  boost::mutex mutex_;
  boost::thread recv_thread_;
  boost::condition condition_;

  volatile bool stop_recv_thread_{false};
  bool recv_thread_running_{false};
  std::string recv_thread_error_msg_;

  geometry_msgs::msg::WrenchStamped new_data_;
  unsigned packet_count_{0};
  unsigned lost_packets_{0};
  unsigned out_of_order_count_{0};

  double force_scale_{1.0};
  double torque_scale_{1.0};

  unsigned diag_packet_count_{0};
  std::chrono::steady_clock::time_point last_diag_pub_tp_;

  uint32_t last_rdt_sequence_{0};
  uint32_t system_status_{0};
};

} // namespace netft_rdt_driver
