#include "media/ExternalInput.h"

#include <boost/cstdint.hpp>
#include <sys/time.h>
#include <arpa/inet.h>
#include <libavutil/time.h>

#include <cstdio>
#include <cstring>

#include "./WebRtcConnection.h"

namespace erizo {
DEFINE_LOGGER(ExternalInput, "media.ExternalInput");
ExternalInput::ExternalInput(const std::string& inputUrl):url_(inputUrl) {
  context_ = NULL;
  running_ = false;
  needTranscoding_ = false;
}

ExternalInput::~ExternalInput() {
  ELOG_DEBUG("Destructor ExternalInput %s" , url_.c_str());
  ELOG_DEBUG("Closing ExternalInput");
  running_ = false;
  thread_.join();
  deliver_thread.join();
  if (needTranscoding_)
    encodeThread_.join();
  av_free_packet(&avpacket_);
  if (context_ != NULL)
    avformat_close_input(&context_);
    avformat_free_context(context_);
  ELOG_DEBUG("ExternalInput closed");
  video_queue_log_.close();
  audio_queue_log_.close();
}

int ExternalInput::init() {
  video_queue_log_.open ("video_queue_log_.txt", std::fstream::in | std::fstream::out | std::fstream::trunc);
  audio_queue_log_.open ("audio_queue_log_.txt", std::fstream::in | std::fstream::out | std::fstream::trunc);
  context_ = avformat_alloc_context();
  av_register_all();
  avcodec_register_all();
  avformat_network_init();
  // open rtsp
  av_init_packet(&avpacket_);
  avpacket_.data = NULL;
  ELOG_DEBUG("Trying to open input from url %s", url_.c_str());
  int res = avformat_open_input(&context_, url_.c_str(), NULL, NULL);
  char errbuff[500];
  ELOG_DEBUG("Opening input result %d", res);
  if (res != 0) {
    av_strerror(res, reinterpret_cast<char*>(&errbuff), 500);
    ELOG_ERROR("Error opening input %s", errbuff);
    return res;
  }
  res = avformat_find_stream_info(context_, NULL);
  if (res < 0) {
    av_strerror(res, reinterpret_cast<char*>(&errbuff), 500);
    ELOG_ERROR("Error finding stream info %s", errbuff);
    return res;
  }

  // Streams
  AVStream *video_st, *audio_st;

  int streamNo = av_find_best_stream(context_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (streamNo < 0) {
    ELOG_WARN("No Video stream found");
  } else {
    media_info_.hasVideo = true;
    video_stream_index_ = streamNo;
    video_st = context_->streams[streamNo];
  }

  int audioStreamNo = av_find_best_stream(context_, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  if (audioStreamNo < 0) {
    ELOG_WARN("No Audio stream found");
  } else {
    media_info_.hasAudio = true;
    audio_stream_index_ = audioStreamNo;
    audio_st = context_->streams[audio_stream_index_];
    ELOG_DEBUG("Has Audio, audio stream number %d. time base = %d / %d. start time = %lld",
               audio_stream_index_, audio_st->time_base.num, audio_st->time_base.den,
               audio_st->start_time);
    audio_time_base_ = audio_st->time_base.den;
    ELOG_DEBUG("Audio Time base %lld", audio_time_base_);
    if (audio_st->codec->codec_id == AV_CODEC_ID_PCM_MULAW) {
      ELOG_DEBUG("PCM U8");
      media_info_.audioCodec.sampleRate = 8000;
      media_info_.audioCodec.codec = AUDIO_CODEC_PCM_U8;
      media_info_.rtpAudioInfo.PT = PCMU_8000_PT;
    } else if (audio_st->codec->codec_id == AV_CODEC_ID_OPUS) {
      ELOG_DEBUG("OPUS");
      media_info_.audioCodec.sampleRate = 48000;
      media_info_.audioCodec.codec = AUDIO_CODEC_OPUS;
      media_info_.rtpAudioInfo.PT = OPUS_48000_PT;
    }
    if (!media_info_.hasVideo)
      video_st = audio_st;
  }

  if (video_st->codec->codec_id == AV_CODEC_ID_VP8 || !media_info_.hasVideo) {
    video_time_base_ = video_st->time_base.den;
    video_avg_frame_rate_ = video_st->avg_frame_rate;
    ELOG_DEBUG("No need for video transcoding, already VP8");
    ELOG_DEBUG("Average Framerate: %d / %d", video_avg_frame_rate_.num, video_avg_frame_rate_.den);
    ELOG_DEBUG("Video Time base: %lld, start time: %lld", video_time_base_, video_st->start_time);
    needTranscoding_ = false;
    decodedBuffer_.reset((unsigned char*) malloc(100000));
    MediaInfo media_info_;
    media_info_.rtpVideoInfo.PT = VP8_90000_PT;
    media_info_.processorType = PACKAGE_ONLY_NO_RESCALE_TS;
    if (audio_st->codec->codec_id == AV_CODEC_ID_PCM_MULAW) {
      ELOG_DEBUG("PCM U8");
      media_info_.audioCodec.sampleRate = 8000;
      media_info_.audioCodec.codec = AUDIO_CODEC_PCM_U8;
      media_info_.rtpAudioInfo.PT = PCMU_8000_PT;
    } else if (audio_st->codec->codec_id == AV_CODEC_ID_OPUS) {
      ELOG_DEBUG("OPUS");
      media_info_.audioCodec.sampleRate = 48000;
      media_info_.audioCodec.codec = AUDIO_CODEC_OPUS;
      media_info_.rtpAudioInfo.PT = OPUS_48000_PT;
    }
    op_.reset(new OutputProcessor());
    op_->init(media_info_, this);
  } else {
    needTranscoding_ = true;
    inCodec_.initDecoder(video_st->codec);

    bufflen_ = video_st->codec->width*video_st->codec->height*3/2;
    decodedBuffer_.reset((unsigned char*) malloc(bufflen_));


    media_info_.processorType = RTP_ONLY;
    media_info_.videoCodec.codec = VIDEO_CODEC_VP8;
    media_info_.videoCodec.bitRate = 1000000;
    media_info_.videoCodec.width = 640;
    media_info_.videoCodec.height = 480;
    media_info_.videoCodec.frameRate = 20;
    media_info_.hasVideo = true;

    media_info_.hasAudio = false;
    if (media_info_.hasAudio) {
      media_info_.audioCodec.sampleRate = 8000;
      media_info_.audioCodec.bitRate = 64000;
    }

    op_.reset(new OutputProcessor());
    op_->init(media_info_, this);
  }

  av_init_packet(&avpacket_);

  running_ = true;
  thread_ = boost::thread(&ExternalInput::receiveLoop, this);
  deliver_thread = boost::thread(&ExternalInput::deliverProcessedPackets, this);
  if (needTranscoding_)
    encodeThread_ = boost::thread(&ExternalInput::encodeLoop, this);
  return true;
}

int ExternalInput::sendPLI() {
  ELOG_DEBUG("SEND PLI")
  return 0;
}


void ExternalInput::receiveRtpData(unsigned char* rtpdata, int len) {
  std::shared_ptr<dataPacket> packet = std::make_shared<dataPacket>(
      0, reinterpret_cast<char*>(rtpdata), len, VIDEO_PACKET);

  AVPacketProcessed processed_packet = AVPacketProcessed{VIDEO_PACKET, packet, video_last_time_pts_};
  enqueuePacket(processed_packet);
}

void ExternalInput::receiveLoop() {
  int gotDecodedFrame = 0;
  int packet_count = 0;
  bool is_realtime = false;
  DataType media_type;
  std::string media_type_string;

  av_read_play(context_);
  delivery_state_.startClock();
  ELOG_DEBUG("Start time set to: %lld", delivery_state_.start_time);

  ELOG_DEBUG("Input Format: %s", context_->iformat->name);
  if (strcmp(context_->iformat->name, "rtsp") == 0) {
    is_realtime = true;
  }

  ELOG_DEBUG("Start playing external input %s", url_.c_str() );
  while (av_read_frame(context_, &avpacket_) >= 0 && running_ == true) {
    packet_count++;

    // Need Transcoding
    if (needTranscoding_ && avpacket_.stream_index == video_stream_index_) {
      inCodec_.decodeVideo(avpacket_.data, avpacket_.size, decodedBuffer_.get(), bufflen_, &gotDecodedFrame);
      RawDataPacket packetR;
      if (gotDecodedFrame) {
        packetR.data = decodedBuffer_.get();
        packetR.length = bufflen_;
        packetR.type = VIDEO;
        queueMutex_.lock();
        packetQueue_.push(packetR);
        queueMutex_.unlock();
        gotDecodedFrame = 0;
      }

    // Already VP8, Opus, PCMU8
    } else {
      int64_t pts = 0;
      int64_t time_pts = 0;
      int64_t timestamp = 0;
      int64_t input_time_base = 0;
      int output_sample_rate = 0;

      // FIXME:
      // Discard AV_NOPTS_VALUE packets

      if (avpacket_.stream_index == video_stream_index_) {
        media_type = VIDEO;
        media_type_string = "VIDEO";
        output_sample_rate = 90000;
        input_time_base = video_time_base_;
      } else if (avpacket_.stream_index == audio_stream_index_) {
        media_type = AUDIO;
        media_type_string = "AUDIO";
        output_sample_rate = 48000;
        input_time_base = audio_time_base_;
      }

      pts = avpacket_.pts;
      if (pts == AV_NOPTS_VALUE || pts == LLONG_MIN || pts == LLONG_MAX) {
        pts = avpacket_.dts;
      }

      time_pts = av_rescale(pts, AV_TIME_BASE, input_time_base);
      timestamp = av_rescale(pts, output_sample_rate, input_time_base);
      //ELOG_DEBUG("Packet PTS: %lld, DTS: %lld, Timestamp: %lld, TimePTS: %lld",
      //    avpacket_.pts, avpacket_.dts, timestamp, time_pts);

      // Video
      if (media_type == VIDEO) {
        // Keep track of last pts in AV_TIME_BASE units, unlike audio we don't have inmediatly
        // after packet creation, receiveRtpData` got called several times from `packageVideo`
        // so we store the last time pts in AV_TIME_BASE for those packets.
        video_last_time_pts_ = time_pts;

        op_->packageVideo(avpacket_.data, avpacket_.size, decodedBuffer_.get(), timestamp);

      // Audio
      } else if (media_type == AUDIO) {

        int length = op_->packageAudio(avpacket_.data, avpacket_.size, decodedBuffer_.get(), timestamp);
        if (length > 0) {
          std::shared_ptr<dataPacket> packet = std::make_shared<dataPacket>(0,
              reinterpret_cast<char*>(decodedBuffer_.get()), length, AUDIO_PACKET);

          AVPacketProcessed processed_packet = AVPacketProcessed{AUDIO_PACKET, packet, time_pts};
          enqueuePacket(processed_packet);
        }
      }
    }
    av_packet_unref(&avpacket_);
  }
  ELOG_DEBUG("Ended stream to play %s", url_.c_str());
  running_ = false;
  av_read_pause(context_);
}

void ExternalInput::enqueuePacket(AVPacketProcessed packet_processed) {
  packet_queue_mutex.lock();
  delivery_state_.packet_queue.push_back(packet_processed);
  std::sort(std::begin(delivery_state_.packet_queue), std::end(delivery_state_.packet_queue));
  packet_queue_mutex.unlock();
}

void ExternalInput::deliverProcessedPackets() {
  while (running_) {
    bool delivered = deliverPacket();
    if (!delivered) {
      boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
    }
  }
}

void ExternalInput::updateClock(packetType type, int64_t value) {
  switch (type) {
    case VIDEO_PACKET:
      delivery_state_.video_clock = std::make_shared<int64_t> (value);
    case AUDIO_PACKET:
      delivery_state_.audio_clock = std::make_shared<int64_t> (value);
      break;
    case OTHER_PACKET:
      break;
  }
}

bool ExternalInput::deliverPacket() {
  if (delivery_state_.packet_queue.size() == 0) {
      return false;
  }

  packet_queue_mutex.lock();

  std::stringstream packet_status_str_buffer;
  bool deliver_packet = true;
  AVPacketProcessed packet = delivery_state_.packet_queue.front();
  int64_t master_clock = delivery_state_.getMasterClock();
  int64_t clock_diff = master_clock - packet.time_pts;
  //int64_t last_packet_diff = packet.time_pts - stream_clock;
  int64_t threshold = packet.packet_type == AUDIO_PACKET ? 20000 : 40000;

  // Is in our range
  if (std::abs(clock_diff) >= 0 && std::abs(clock_diff) < threshold ) {
    deliver_packet = true;

  // Too soon, not deliver.
  } else if (clock_diff < 0) {
    deliver_packet = false;
    packet_status_str_buffer << "SOON, PUT BACK";

  // Too late, not deliver, remove.
  } else if (clock_diff > 0) {
    deliver_packet = false;
    int dropped_count = 0;
    while(delivery_state_.packet_queue.size() > 0 && delivery_state_.packet_queue.front().time_pts < master_clock) {
      delivery_state_.packet_queue.pop_front();
      dropped_count++;
    }
    packet_status_str_buffer << "DROPPED: " << dropped_count;
    delivery_state_.setMasterClock(clock_diff);
  }

  if (deliver_packet) {
    delivery_state_.packet_queue.pop_front();

    if (packet.packet_type == VIDEO_PACKET && video_sink_ != nullptr) {
      video_sink_->deliverVideoData(packet.rtp_packet);

    } else if (packet.packet_type == AUDIO_PACKET && audio_sink_ != nullptr) {
      audio_sink_->deliverAudioData(packet.rtp_packet);
      //delivery_state_.setMasterClock(clock_diff);
    }

    //updateClock(packet.packet_type, packet.time_pts);
    delivery_state_.delivered_count++;
    packet_status_str_buffer << "DELIVERED";
  }

  /*
   * Uncomment this will cause every packet to be logged in a separated file.
  std::fstream *log_stream = packet.packet_type == VIDEO_PACKET ? &video_queue_log_ : &audio_queue_log_;
  *log_stream << "Q Size: " << delivery_state_.packet_queue.size() << ", Status: " << packet_status_str_buffer.str() << ", PTS: " \
    << time_pts << ", MClock: " << master_clock << ", New MClock: " << delivery_state_.getMasterClock() << \
    ", Diff: " << clock_diff << std::endl;
  */


  packet_queue_mutex.unlock();

  return deliver_packet;
}

void ExternalInput::encodeLoop() {
  while (running_ == true) {
    queueMutex_.lock();
    if (packetQueue_.size() > 0) {
      op_->receiveRawData(packetQueue_.front());
      packetQueue_.pop();
      queueMutex_.unlock();
    } else {
      queueMutex_.unlock();
      usleep(10000);
    }
  }
}
}  // namespace erizo
