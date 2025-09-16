#include "audio_encoder.h"
#include "frame_queue.h"
#include "packet_queue.h"
#include <iostream>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/mathematics.h"
}

void audio_encoder(AVCodecContext* codec_ctx,
                  FrameQueue& frame_queue,
                  PacketQueue& packet_queue) {
    if (!codec_ctx) {
        std::cerr << "音频编码器上下文为空，无法进行编码" << std::endl;
        packet_queue.set_eof();
        return;
    }

    AVPacket* pkt = av_packet_alloc();
    int frame_count = 0;
    int packet_count = 0;
    
    // std::cout << "音频编码器开始工作，采样率: " << codec_ctx->sample_rate
    //           << ", 声道数: " << codec_ctx->channels
    //           << ", 时间基准: " << codec_ctx->time_base.num << "/" << codec_ctx->time_base.den
    //           << ", 帧大小: " << codec_ctx->frame_size << std::endl;
    
    // 检查编码器是否需要特定的帧大小
    if (codec_ctx->frame_size > 0) {
        std::cout << "音频编码器要求固定帧大小: " << codec_ctx->frame_size << " 采样" << std::endl;
    } else {
        std::cout << "音频编码器接受可变帧大小" << std::endl;
    }
    
    while(AVFrame* frame = frame_queue.pop()) {
        frame_count++;
        
        // 打印输入帧的时间戳和采样数
        // std::cout << "处理音频帧 #" << frame_count
        //           << " PTS: " << frame->pts
        //           << " 采样数: " << frame->nb_samples
        //           << " 时间(秒): " << frame->pts * av_q2d(codec_ctx->time_base) << std::endl;
        
        // 检查帧大小是否符合编码器要求
        if (codec_ctx->frame_size > 0 && frame->nb_samples != codec_ctx->frame_size) {
            std::cerr << "帧采样数 (" << frame->nb_samples
                      << ") 与编码器要求的帧大小 (" << codec_ctx->frame_size 
                      << ") 不匹配" << std::endl;
        }
        
        // 发送帧到编码器
        int ret = avcodec_send_frame(codec_ctx, frame);
        if(ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "发送音频帧到编码器失败: " << errbuf << std::endl;
            
            // 打印更多帧信息以便调试
            std::cerr << "帧详情: 采样格式=" << av_get_sample_fmt_name(static_cast<AVSampleFormat>(frame->format))
                      << ", 声道数=" << frame->channels
                      << ", 声道布局=0x" << std::hex << frame->channel_layout << std::dec
                      << ", 采样率=" << frame->sample_rate << std::endl;
            
            av_frame_free(&frame);
            continue;
        }

        // 接收编码后的包
        while(true) {
            ret = avcodec_receive_packet(codec_ctx, pkt);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            
            if(ret < 0) {
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errbuf, sizeof(errbuf));
                std::cerr << "从音频编码器接收包失败: " << errbuf << std::endl;
                break;
            }
            
            packet_count++;
            
            // 确保包有正确的时间戳
            if (pkt->pts == AV_NOPTS_VALUE) {
                std::cerr << "音频编码器输出的包没有PTS" << std::endl;
                av_packet_unref(pkt);
                continue;
            }
            
            // 打印编码后的数据包信息
            std::cout << "编码后的音频包 #" << packet_count 
                      << " PTS: " << pkt->pts 
                      << " DTS: " << pkt->dts 
                      << " 时间(秒): " << pkt->pts * av_q2d(codec_ctx->time_base)
                      << " 大小: " << pkt->size << " 字节" << std::endl;
            
            AVPacket* cloned = av_packet_alloc();
            av_packet_ref(cloned, pkt);
            
            // 确保音频包的流索引正确
            cloned->stream_index = 1; // 音频流通常是第二个流
            
            packet_queue.push(cloned);
            av_packet_unref(pkt);
        }
        av_frame_free(&frame);
    }

    std::cout << "音频帧处理完成，共处理 " << frame_count << " 帧，编码 " << packet_count << " 个包" << std::endl;

    // 冲洗编码器
    std::cout << "开始冲洗音频编码器..." << std::endl;
    avcodec_send_frame(codec_ctx, nullptr);
    int flush_count = 0;
    
    while(true) {
        int ret = avcodec_receive_packet(codec_ctx, pkt);
        if(ret == AVERROR_EOF) {
            std::cout << "音频编码器已冲洗完毕" << std::endl;
            break;
        }
        if(ret == AVERROR(EAGAIN)) {
            std::cout << "音频编码器需要更多数据" << std::endl;
            break;
        }
        if(ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "冲洗音频编码器失败: " << errbuf << std::endl;
            break;
        }
        
        flush_count++;
        packet_count++;
        
        // 确保包有正确的时间戳
        if (pkt->pts == AV_NOPTS_VALUE) {
            std::cerr << "冲洗时音频编码器输出的包没有PTS" << std::endl;
            av_packet_unref(pkt);
            continue;
        }
        
        std::cout << "冲洗音频编码器，获取到包 #" << flush_count 
                  << " PTS: " << pkt->pts 
                  << " DTS: " << pkt->dts 
                  << " 时间(秒): " << pkt->pts * av_q2d(codec_ctx->time_base)
                  << " 大小: " << pkt->size << " 字节" << std::endl;
        
        AVPacket* cloned = av_packet_alloc();
        av_packet_ref(cloned, pkt);
        
        // 确保音频包的流索引正确,索引错误容易导致muxer无法写入
        cloned->stream_index = 1;
        
        packet_queue.push(cloned);
        av_packet_unref(pkt);
    }
    
    std::cout << "音频编码器冲洗完成" << std::endl;
    packet_queue.set_eof();
    av_packet_free(&pkt);
}



