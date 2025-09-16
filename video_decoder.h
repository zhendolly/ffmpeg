#ifndef VIDEO_DECODER_H
#define VIDEO_DECODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"

#ifdef __cplusplus
}
#endif

#include "packet_queue.h"
#include "frame_queue.h"

void video_decoder(AVCodecContext* codec_ctx, PacketQueue& packet_queue, FrameQueue& frame_queue);

#endif