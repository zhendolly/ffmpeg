#ifndef AUDIO_DECODER_H
#define AUDIO_DECODER_H
#include <queue>
#include <mutex>
#include <condition_variable>
#include "packet_queue.h"
#include "frame_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"

#ifdef __cplusplus
}
#endif

extern std::queue<AVFrame*> audio_frame_queue;
extern std::mutex audio_mutex;
extern std::condition_variable audio_cv;
extern bool audio_decoding_finished;

void audio_decoder(AVCodecContext* codec_ctx, PacketQueue& packet_queue, FrameQueue& frame_queue);



#endif

