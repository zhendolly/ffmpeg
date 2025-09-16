#ifndef AUDIO_ENCODER_H
#define AUDIO_ENCODER_H


extern "C" {
#include <libavcodec/avcodec.h>
}

class FrameQueue;
class PacketQueue;

void audio_encoder(AVCodecContext* codec_ctx, FrameQueue& frame_queue,PacketQueue& packet_queue);

#endif

