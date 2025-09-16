#include "video_filter.h"
#include <iostream>


extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include <stddef.h>
}

// 初始化滤波器图
int init_filter_graph(AVCodecContext* dec_ctx, AVFilterGraph** filter_graph, AVFilterContext** buffer_src_ctx, AVFilterContext** buffer_sink_ctx, float speed) {
    // 创建滤波器图
    *filter_graph = avfilter_graph_alloc();
    if (!*filter_graph) {
        std::cerr << "无法创建滤波器图" << std::endl;
        return -1;
    }

    // 获取输入和输出滤波器
    const AVFilter* buffer_src = avfilter_get_by_name("buffer");
    const AVFilter* buffer_sink = avfilter_get_by_name("buffersink");

    // 创建输入滤波器上下文
    char args[512];
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=1/1",
             dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
             dec_ctx->time_base.num, dec_ctx->time_base.den);
    if (avfilter_graph_create_filter(buffer_src_ctx, buffer_src, "in", args, nullptr, *filter_graph) < 0) {
        std::cerr << "无法创建输入滤波器" << std::endl;
        return -1;
    }

    // 创建输出滤波器上下文
    if (avfilter_graph_create_filter(buffer_sink_ctx, buffer_sink, "out", nullptr, nullptr, *filter_graph) < 0) {
        std::cerr << "无法创建输出滤波器" << std::endl;
        return -1;
    }

    // 创建setpts滤波器用于变速
    AVFilterContext* setpts_ctx;
    const AVFilter* setpts = avfilter_get_by_name("setpts");
    char setpts_args[64];
    snprintf(setpts_args, sizeof(setpts_args), "PTS/%f", speed); // 根据传入的速度参数调整
    if (avfilter_graph_create_filter(&setpts_ctx, setpts, "setpts", setpts_args, nullptr, *filter_graph) < 0) {
        std::cerr << "无法创建setpts滤波器" << std::endl;
        return -1;
    }

    std::cout << "视频变速滤波器创建成功，速度因子: " << speed << std::endl;

    // 连接滤波器：输入 -> setpts -> 输出
    if (avfilter_link(*buffer_src_ctx, 0, setpts_ctx, 0) != 0 ||
        avfilter_link(setpts_ctx, 0, *buffer_sink_ctx, 0) != 0) {
        std::cerr << "无法连接滤波器" << std::endl;
        return -1;
    }

    // 初始化滤波器图
    if (avfilter_graph_config(*filter_graph, nullptr) < 0) {
        std::cerr << "无法初始化滤波器图" << std::endl;
        return -1;
    }

    return 0;
}

//处理视频帧
int filter_frame(AVFilterContext* buffer_src_ctx, AVFilterContext* buffer_sink_ctx, FrameQueue& frame_queue, AVCodecContext* enc_ctx, PacketQueue& mux_queue) {
    AVFrame* filtered_frame = av_frame_alloc();
    if (!filtered_frame) {
        std::cerr << "无法分配帧" << std::endl;
        return -1;
    }

    int frame_count = 0;
    int encoded_count = 0;

    while (AVFrame* frame = frame_queue.pop()) {
        frame_count++;


        //std::cout << "进入while循环" << std::endl;

        // 打印输入帧的时间戳
        // std::cout << "处理视频帧 #" << frame_count
        //           << " PTS: " << frame->pts
        //           << " 时间(秒): " << frame->pts * av_q2d(enc_ctx->time_base) << std::endl;
        
        // 将帧发送到滤波器图
        if (av_buffersrc_add_frame(buffer_src_ctx, frame) < 0) {
            std::cerr << "无法发送帧到滤波器图" << std::endl;
            av_frame_free(&frame);
            break;
        }

        // 从滤波器图获取处理后的帧
        while (true) {
            int ret = av_buffersink_get_frame(buffer_sink_ctx, filtered_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;

            if (ret < 0) {
                std::cerr << "无法从滤波器图获取帧" << std::endl;
                break;
            }

            //std::cout << "1357" << std::endl;

            // 打印滤波后帧的时间戳
            // std::cout << "滤波后的帧 PTS: " << filtered_frame->pts
            //           << " 时间(秒): " << filtered_frame->pts * av_q2d(enc_ctx->time_base) << std::endl;

            // 将处理后的帧发送到编码器
            ret = avcodec_send_frame(enc_ctx, filtered_frame);
            if (ret < 0) {
                std::cerr << "发送帧到编码器失败: " << ret << std::endl;
                break;
            }

            // 从编码器获取数据包
            AVPacket* pkt = av_packet_alloc();
            while (true) {
                ret = avcodec_receive_packet(enc_ctx, pkt);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;

                if (ret < 0) {
                    std::cerr << "从编码器获取数据包失败: " << ret << std::endl;
                    break;
                }

                encoded_count++;
                
                // 确保包有正确的时间戳
                if (pkt->pts == AV_NOPTS_VALUE) {
                    std::cerr << "编码器输出的包没有PTS" << std::endl;
                    av_packet_unref(pkt);
                    continue;
                }

                // 打印编码后的数据包信息
                // std::cout << "编码后的视频包 #" << encoded_count
                //           << " PTS: " << pkt->pts
                //           << " DTS: " << pkt->dts
                //           << " 时间(秒): " << pkt->pts * av_q2d(enc_ctx->time_base)
                //           << " 大小: " << pkt->size << " 字节" << std::endl;

                // 将数据包推送到复用队列
                AVPacket* pkt_copy = av_packet_alloc();
                av_packet_ref(pkt_copy, pkt);
                mux_queue.push(pkt_copy);

                // 释放原始数据包
                av_packet_unref(pkt);
            }

            // 释放数据包
            av_packet_free(&pkt);

            // 释放处理后的帧
            av_frame_unref(filtered_frame);
        }

        // 释放原始帧
        av_frame_free(&frame);
    }

    // std::cout << "视频滤波处理完成，共处理 " << frame_count << " 帧，编码 " << encoded_count << " 个包" << std::endl;

    // 释放处理后的帧
    av_frame_free(&filtered_frame);
    return 0;
}

