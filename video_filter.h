#ifndef VIDEO_FILTER_H
#define VIDEO_FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>

#ifdef __cplusplus
}
#endif

#include "frame_queue.h"
#include "packet_queue.h"

// 初始化滤波器图
int init_filter_graph(AVCodecContext* dec_ctx, AVFilterGraph** filter_graph, AVFilterContext** buffer_src_ctx, AVFilterContext** buffer_sink_ctx, float speed);

// 处理视频帧
//int filter_frame(AVFilterContext* buffer_src_ctx, AVFilterContext* buffer_sink_ctx, FrameQueue& frame_queue, AVCodecContext* enc_ctx);
int filter_frame(AVFilterContext* buffer_src_ctx, AVFilterContext* buffer_sink_ctx, FrameQueue& frame_queue, AVCodecContext* enc_ctx, PacketQueue& mux_queue);

#endif
