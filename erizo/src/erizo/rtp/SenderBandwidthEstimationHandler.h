#ifndef ERIZO_SRC_ERIZO_RTP_SENDERBANDWIDTHESTIMATIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_SENDERBANDWIDTHESTIMATIONHANDLER_H_
#include "pipeline/Handler.h"
#include "./logger.h"
#include "./WebRtcConnection.h"
#include "./rtp/RtcpProcessor.h"
#include "lib/Clock.h"

#include "webrtc/modules/bitrate_controller/send_side_bandwidth_estimation.h"

namespace erizo {
using webrtc::SendSideBandwidthEstimation;


class SenderBandwidthEstimationListener {
 public:
  virtual ~SenderBandwidthEstimationListener() {}
  virtual void onBandwidthEstimate(int estimated_bitrate, uint8_t estimated_loss,
      int64_t estimated_rtt) = 0;
};

class SenderBandwidthEstimationHandler : public Handler,
  public std::enable_shared_from_this<SenderBandwidthEstimationHandler> {
  DECLARE_LOGGER();

 public:
  static const uint16_t kMaxSrListSize = 20;
  static const uint32_t kStartSendBitrate = 300000;
  static constexpr duration kMinUpdateEstimateInterval = std::chrono::milliseconds(25);

 public:
  explicit SenderBandwidthEstimationHandler(std::shared_ptr<Clock> the_clock = std::make_shared<SteadyClock>());
  explicit SenderBandwidthEstimationHandler(const SenderBandwidthEstimationHandler&& handler);  // NOLINT
  virtual ~SenderBandwidthEstimationHandler() {}

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "sender_bwe";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

  void analyzeSr(RtcpHeader *head);

  void setListener(SenderBandwidthEstimationListener* listener) {
    bwe_listener_ = listener;
  }

 private:
  WebRtcConnection* connection_;
  SenderBandwidthEstimationListener* bwe_listener_;
  std::shared_ptr<Clock> clock_;
  bool initialized_;
  bool enabled_;
  bool received_remb_;
  uint32_t period_packets_sent_;
  int estimated_bitrate_;
  uint8_t estimated_loss_;
  int64_t estimated_rtt_;
  time_point last_estimate_update_;
  std::shared_ptr<SendSideBandwidthEstimation> sender_bwe_;
  std::list<std::shared_ptr<SrDelayData>> sr_delay_data_;
  std::shared_ptr<Stats> stats_;

  void updateEstimate();
};
}  // namespace erizo
#endif  //  ERIZO_SRC_ERIZO_RTP_SENDERBANDWIDTHESTIMATIONHANDLER_H_
