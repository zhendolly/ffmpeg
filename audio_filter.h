#ifndef AUDIO_FILTER_H
#define AUDIO_FILTER_H

#include "frame_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <libavfilter/avfilter.h>
#include <libavcodec/avcodec.h>

#ifdef __cplusplus
}
#endif

int init_audio_filters(AVCodecContext* dec_ctx,
                      AVCodecContext* enc_ctx,
                      AVFilterGraph** graph,
                      AVFilterContext** src_ctx,
                      AVFilterContext** sink_ctx,
                      float speed);

void audio_filter_process(AVFilterContext* src_ctx,
                         AVFilterContext* sink_ctx,
                         FrameQueue& input_queue,
                         FrameQueue& output_queue);

#endif


