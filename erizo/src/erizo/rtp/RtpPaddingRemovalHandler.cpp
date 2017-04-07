#include "rtp/RtpPaddingRemovalHandler.h"
#include "rtp/RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(RtpPaddingRemovalHandler, "rtp.RtpPaddingRemovalHandler");

RtpPaddingRemovalHandler::RtpPaddingRemovalHandler()
  : enabled_{true}, initialized_{false} {}

void RtpPaddingRemovalHandler::enable() {
  enabled_ = true;
}

void RtpPaddingRemovalHandler::disable() {
  enabled_ = false;
}

void RtpPaddingRemovalHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);

  if (!chead->isRtcp() && enabled_ && packet->type == VIDEO_PACKET) {
    if (!removePaddingBytes(packet)) {
      return;
    }
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    uint16_t sequence_number = rtp_header->getSeqNumber();
    SequenceNumber sequence_number_info = translator_.get(sequence_number, false);

    if (sequence_number_info.type != SequenceNumberType::Valid) {
      return;
    }
    rtp_header->setSeqNumber(sequence_number_info.output);
  }
  ctx->fireRead(packet);
}

void RtpPaddingRemovalHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  ctx->fireWrite(packet);
}

bool RtpPaddingRemovalHandler::removePaddingBytes(std::shared_ptr<dataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  int header_length = rtp_header->getHeaderLength();

  int padding_length = RtpUtils::getPaddingLength(packet);
  if (padding_length + header_length == packet->length) {
    uint16_t sequence_number = rtp_header->getSeqNumber();
    translator_.get(sequence_number, true);
    return false;
  }
  packet->length -= padding_length;
  rtp_header->padding = 0;
  return true;
}

void RtpPaddingRemovalHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (!pipeline) {
    return;
  }

  if (initialized_) {
    return;
  }
  initialized_ = true;
}
}  // namespace erizo
