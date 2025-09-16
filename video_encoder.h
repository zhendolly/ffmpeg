#ifndef VIDEO_ENCODER_H
#define VIDEO_ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"

#ifdef __cplusplus
}
#endif

#include "frame_queue.h"
#include "packet_queue.h"

void video_encoder(AVCodecContext* enc_ctx, FrameQueue& frame_queue, PacketQueue& mux_queue, float speed);

#endif


