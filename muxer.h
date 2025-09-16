#ifndef MUXER_H
#define MUXER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"

#ifdef __cplusplus
}
#endif

#include "packet_queue.h"

void muxer(AVFormatContext* out_fmt,
          PacketQueue& video_queue,
          PacketQueue& audio_queue);


#endif
