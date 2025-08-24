#include "franka_ft_sensor/netft_rdt_driver.h"

#include <rclcpp/rclcpp.hpp>
#include <diagnostic_msgs/msg/diagnostic_status.hpp>
#include <cstdint>

using boost::asio::ip::udp;

namespace netft_rdt_driver {

static inline rclcpp::Logger logger() {
  return rclcpp::get_logger("netft_rdt_driver");
}

struct RDTRecord {
  uint32_t rdt_sequence_;
  uint32_t ft_sequence_;
  uint32_t status_;
  int32_t fx_, fy_, fz_;
  int32_t tx_, ty_, tz_;

  enum { RDT_RECORD_SIZE = 36 };

  static uint32_t unpack32(const uint8_t *buffer) {
    return (uint32_t(buffer[0]) << 24) | (uint32_t(buffer[1]) << 16) |
           (uint32_t(buffer[2]) << 8) | (uint32_t(buffer[3]) << 0);
  }

  void unpack(const uint8_t *buffer) {
    rdt_sequence_ = unpack32(buffer + 0);
    ft_sequence_  = unpack32(buffer + 4);
    status_       = unpack32(buffer + 8);
    fx_ = unpack32(buffer + 12);
    fy_ = unpack32(buffer + 16);
    fz_ = unpack32(buffer + 20);
    tx_ = unpack32(buffer + 24);
    ty_ = unpack32(buffer + 28);
    tz_ = unpack32(buffer + 32);
  }
};

struct RDTCommand {
  uint16_t command_header_{HEADER};
  uint16_t command_{0};
  uint32_t sample_count_{0};

  enum { HEADER = 0x1234 };
  enum {
    CMD_STOP_STREAMING = 0,
    CMD_START_HIGH_SPEED_STREAMING = 2
  };
  enum { INFINITE_SAMPLES = 0 };
  enum { RDT_COMMAND_SIZE = 8 };

  void pack(uint8_t *buffer) const {
    // big-endian
    buffer[0] = (command_header_ >> 8) & 0xFF;
    buffer[1] = (command_header_ >> 0) & 0xFF;
    buffer[2] = (command_        >> 8) & 0xFF;
    buffer[3] = (command_        >> 0) & 0xFF;
    buffer[4] = (sample_count_   >> 8) & 0xFF;
    buffer[5] = (sample_count_   >> 0) & 0xFF;
    buffer[6] = (sample_count_   >> 8) & 0xFF;
    buffer[7] = (sample_count_   >> 0) & 0xFF;
  }
};

NetFTRDTDriver::NetFTRDTDriver(const std::string &address)
: address_(address), socket_(io_service_) {
  // UDP connect to NetFT (RDT port 49152)
  udp::endpoint netft_endpoint(
      boost::asio::ip::address_v4::from_string(address), RDT_PORT);
  socket_.open(udp::v4());
  socket_.connect(netft_endpoint);

  // scales (keep your old placeholders 1e6 counts)
  static const double counts_per_force  = 1000000.0;
  static const double counts_per_torque = 1000000.0;
  force_scale_  = 1.0 / counts_per_force;
  torque_scale_ = 1.0 / counts_per_torque;

  last_diag_pub_tp_ = std::chrono::steady_clock::now();

  // Start receive thread and kick off streaming
  recv_thread_ = boost::thread(&NetFTRDTDriver::recvThreadFunc, this);
  for (int i = 0; i < 10; ++i) {
    startStreaming();
    if (waitForNewData()) break;
  }
  {
    boost::unique_lock<boost::mutex> lock(mutex_);
    if (packet_count_ == 0) {
      throw std::runtime_error("No data received from NetFT device");
    }
  }
}

NetFTRDTDriver::~NetFTRDTDriver() {
  stop_recv_thread_ = true;
  if (!recv_thread_.timed_join(boost::posix_time::seconds(1))) {
    RCLCPP_WARN(logger(), "Interrupting recv thread");
    recv_thread_.interrupt();
    if (!recv_thread_.timed_join(boost::posix_time::seconds(1))) {
      RCLCPP_WARN(logger(), "Failed second join to recv thread");
    }
  }
  socket_.close();
}

bool NetFTRDTDriver::waitForNewData() {
  bool got_new_data = false;
  {
    boost::mutex::scoped_lock lock(mutex_);
    unsigned current_packet_count = packet_count_;
    condition_.timed_wait(lock, boost::posix_time::milliseconds(100));
    got_new_data = packet_count_ != current_packet_count;
  }
  return got_new_data;
}

void NetFTRDTDriver::startStreaming() {
  RDTCommand start_transmission;
  start_transmission.command_      = RDTCommand::CMD_START_HIGH_SPEED_STREAMING;
  start_transmission.sample_count_ = RDTCommand::INFINITE_SAMPLES;
  uint8_t buffer[RDTCommand::RDT_COMMAND_SIZE];
  start_transmission.pack(buffer);
  socket_.send(boost::asio::buffer(buffer, RDTCommand::RDT_COMMAND_SIZE));
}

void NetFTRDTDriver::recvThreadFunc() {
  try {
    recv_thread_running_ = true;
    RDTRecord rdt_record;
    geometry_msgs::msg::WrenchStamped tmp;
    uint8_t buffer[RDTRecord::RDT_RECORD_SIZE + 1];

    while (!stop_recv_thread_) {
      size_t len = socket_.receive(
          boost::asio::buffer(buffer, RDTRecord::RDT_RECORD_SIZE + 1));
      if (len != RDTRecord::RDT_RECORD_SIZE) {
        RCLCPP_WARN(logger(), "Received %d bytes, expected %d",
                    static_cast<int>(len), int(RDTRecord::RDT_RECORD_SIZE));
        continue;
      }

      rdt_record.unpack(buffer);
      if (rdt_record.status_ != 0) {
        boost::unique_lock<boost::mutex> lock(mutex_);
        system_status_ = rdt_record.status_;
      }

      int32_t seqdiff = int32_t(rdt_record.rdt_sequence_ - last_rdt_sequence_);
      last_rdt_sequence_ = rdt_record.rdt_sequence_;

      if (seqdiff < 1) {
        boost::unique_lock<boost::mutex> lock(mutex_);
        ++out_of_order_count_;
        continue;
      }

      // Fill wrench (header stamp will be added in the ROS2 node)
      tmp.header.frame_id = "base_link";
      tmp.wrench.force.x  = double(rdt_record.fx_) * force_scale_;
      tmp.wrench.force.y  = double(rdt_record.fy_) * force_scale_;
      tmp.wrench.force.z  = double(rdt_record.fz_) * force_scale_;
      tmp.wrench.torque.x = double(rdt_record.tx_) * torque_scale_;
      tmp.wrench.torque.y = double(rdt_record.ty_) * torque_scale_;
      tmp.wrench.torque.z = double(rdt_record.tz_) * torque_scale_;

      {
        boost::unique_lock<boost::mutex> lock(mutex_);
        new_data_ = tmp;
        lost_packets_ += (seqdiff - 1);
        ++packet_count_;
        condition_.notify_all();
      }
    }
  } catch (const std::exception &e) {
    recv_thread_running_ = false;
    {
      boost::unique_lock<boost::mutex> lock(mutex_);
      recv_thread_error_msg_ = e.what();
    }
  }
}

void NetFTRDTDriver::getData(geometry_msgs::msg::WrenchStamped &data) {
  boost::unique_lock<boost::mutex> lock(mutex_);
  data = new_data_;
}

void NetFTRDTDriver::diagnostics(diagnostic_updater::DiagnosticStatusWrapper &d) {
  d.name = "NetFT RDT Driver : " + address_;
  d.summary(d.OK, "OK");
  d.hardware_id = "0";

  if (diag_packet_count_ == packet_count_) {
    d.mergeSummary(d.ERROR, "No new data in last second");
  }
  if (!recv_thread_running_) {
    d.mergeSummaryf(d.ERROR, "Receive thread has stopped : %s",
                    recv_thread_error_msg_.c_str());
  }
  if (system_status_ != 0) {
    d.mergeSummaryf(d.ERROR, "NetFT reports error 0x%08x", system_status_);
  }

  // rate since last call
  const auto now = std::chrono::steady_clock::now();
  const double dt = std::chrono::duration<double>(now - last_diag_pub_tp_).count();
  const double recv_rate = dt > 0.0 ? double(int32_t(packet_count_ - diag_packet_count_)) / dt : 0.0;

  d.clear();
  d.addf("IP Address", "%s", address_.c_str());
  d.addf("System status", "0x%08x", system_status_);
  d.addf("Good packets", "%u", packet_count_);
  d.addf("Lost packets", "%u", lost_packets_);
  d.addf("Out-of-order packets", "%u", out_of_order_count_);
  d.addf("Recv rate (pkt/sec)", "%.2f", recv_rate);
  d.addf("Force scale (N/bit)", "%f", force_scale_);
  d.addf("Torque scale (Nm/bit)", "%f", torque_scale_);

  geometry_msgs::msg::WrenchStamped data;
  getData(data);
  d.addf("Force X (N)", "%f", data.wrench.force.x);
  d.addf("Force Y (N)", "%f", data.wrench.force.y);
  d.addf("Force Z (N)", "%f", data.wrench.force.z);
  d.addf("Torque X (Nm)", "%f", data.wrench.torque.x);
  d.addf("Torque Y (Nm)", "%f", data.wrench.torque.y);
  d.addf("Torque Z (Nm)", "%f", data.wrench.torque.z);

  last_diag_pub_tp_   = now;
  diag_packet_count_  = packet_count_;
}

} // namespace netft_rdt_driver
