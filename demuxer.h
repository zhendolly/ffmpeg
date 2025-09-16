#ifndef DEMUXER_H
#define DEMUXER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"

#ifdef __cplusplus
}
#endif

#include "packet_queue.h"

void demuxer(AVFormatContext* fmt_ctx,
            PacketQueue& video_queue,
            PacketQueue& audio_queue,
            int video_stream,
            int audio_stream);


#endif
