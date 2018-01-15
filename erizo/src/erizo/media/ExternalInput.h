#ifndef ERIZO_SRC_ERIZO_MEDIA_EXTERNALINPUT_H_
#define ERIZO_SRC_ERIZO_MEDIA_EXTERNALINPUT_H_

#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
}

#include <string>
#include <map>
#include <queue>
#include <climits>

#include "codecs/VideoCodec.h"
#include "./MediaDefinitions.h"
#include "media/MediaProcessor.h"
#include "./logger.h"

#include <fstream>

namespace erizo {

struct AVPacketProcessed {
  packetType packet_type;
  std::shared_ptr<DataPacket> rtp_packet;
  int64_t time_pts;
  bool operator<(AVPacketProcessed const &other) const {
    return time_pts < other.time_pts;
  }
};

struct PacketDeliveryState {
  int64_t start_time = -1;
  std::shared_ptr<int64_t> audio_clock = std::make_shared<int64_t> (0);
  std::shared_ptr<int64_t> video_clock = std::make_shared<int64_t> (0);
  int64_t delivered_count = 0;
  std::deque<AVPacketProcessed> packet_queue;
  int64_t startClock() {
    start_time = av_gettime();
    return start_time;
  }
  int64_t getMasterClock() {
    return av_gettime() - start_time;
  }
  void setMasterClock(int64_t clock) {
    start_time += clock;
  }
};

class ExternalInput : public MediaSource, public RTPDataReceiver {
  DECLARE_LOGGER();

 public:
  explicit ExternalInput(const std::string& inputUrl);
  virtual ~ExternalInput();
  int init();
  void receiveRtpData(unsigned char* rtpdata, int len) override;
  int sendPLI() override;

  void close() override {}

 private:
  boost::scoped_ptr<OutputProcessor> op_;
  VideoDecoder inCodec_;
  MediaInfo media_info_;
  boost::scoped_array<unsigned char> decodedBuffer_;

  std::string url_;
  bool running_;
  bool needTranscoding_;
  boost::mutex queueMutex_;
  boost::thread thread_, encodeThread_;
  std::queue<RawDataPacket> packetQueue_;
  AVFormatContext* context_;
  AVPacket avpacket_;
  int bufflen_;
  int video_stream_index_;
  int audio_stream_index_;
  int64_t video_time_base_;
  int64_t audio_time_base_;
  AVRational video_avg_frame_rate_;

  void receiveLoop();
  void encodeLoop();

  int64_t video_last_time_pts_;
  PacketDeliveryState delivery_state_;
  boost::thread deliver_thread;
  boost::mutex packet_queue_mutex;
  void syncQueueWithClock(std::deque<AVPacketProcessed> *queue, int64_t master_clock);
  void enqueuePacket(AVPacketProcessed packet);
  void deliverProcessedPackets();
  bool deliverPacket();
  void updateClock(packetType type, int64_t value);
  std::fstream video_queue_log_;
  std::fstream audio_queue_log_;
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_MEDIA_EXTERNALINPUT_H_
