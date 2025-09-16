#include "video_encoder.h"
#include "video_filter.h"
#include "packet_queue.h"
#include <iostream>


void video_encoder(AVCodecContext* enc_ctx, FrameQueue& frame_queue, PacketQueue& mux_queue, float speed) {
    // 初始化滤波器图
    AVFilterGraph* filter_graph = nullptr;
    AVFilterContext* buffer_src_ctx = nullptr;
    AVFilterContext* buffer_sink_ctx = nullptr;
    if (init_filter_graph(enc_ctx, &filter_graph, &buffer_src_ctx, &buffer_sink_ctx, speed) < 0) {
        std::cerr << "初始化滤波器图失败" << std::endl;
        return;
    }

    std::cout << "视频滤波器图初始化成功，速度: " << speed << std::endl;

    // 处理视频帧
    if (filter_frame(buffer_src_ctx, buffer_sink_ctx, frame_queue, enc_ctx, mux_queue) < 0) {
        std::cerr << "处理视频帧失败" << std::endl;
        return;
    }

    std::cout << "视频帧处理完成" << std::endl;

    // 释放滤波器图
    avfilter_graph_free(&filter_graph);

    std::cout << "视频滤波器图已释放" << std::endl;

    // 冲洗编码器
    avcodec_send_frame(enc_ctx, nullptr);
    AVPacket* pkt = av_packet_alloc();
    int max_flush_attempts = 100; // 最大冲洗尝试次数
    int flush_count = 0;

    while (max_flush_attempts-- > 0) {
        int ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR_EOF) break;
        if (ret == AVERROR(EAGAIN)) continue;

        flush_count++;
        
        // 确保包有正确的时间戳
        if (pkt->pts == AV_NOPTS_VALUE) {
            std::cerr << "编码器输出的包没有PTS" << std::endl;
            continue;
        }

        // std::cout << "冲洗编码器，获取到视频包 #" << flush_count
        //           << " PTS: " << pkt->pts
        //           << " DTS: " << pkt->dts
        //           << " 大小: " << pkt->size << " 字节" << std::endl;

        AVPacket* pkt_copy = av_packet_alloc();
        av_packet_ref(pkt_copy, pkt);
        mux_queue.push(pkt_copy);
        av_packet_unref(pkt);
    }

    std::cout << "视频编码器冲洗完成" << std::endl;
    mux_queue.set_eof();
    av_packet_free(&pkt);
}

