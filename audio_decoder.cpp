#include "audio_decoder.h"
#include <iostream>

extern "C" {
#include "libavcodec/avcodec.h"
}

void audio_decoder(AVCodecContext* codec_ctx, 
                  PacketQueue& packet_queue,
                  FrameQueue& frame_queue) {
    AVFrame* frame = av_frame_alloc();
    
    while(true) {
        AVPacket* pkt = packet_queue.pop();
        if(!pkt) break;


        if(avcodec_send_packet(codec_ctx, pkt) < 0) {
            av_packet_free(&pkt);
            continue;
        }


        while(true) {
            int ret = avcodec_receive_frame(codec_ctx, frame);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;  


            // 克隆解码出的音频帧
            AVFrame* cloned = av_frame_clone(frame);

            // 将音频帧压入帧队列
            frame_queue.push(cloned);
        }

        // 释放数据包内存
        av_packet_free(&pkt);
    }

    // 设置帧队列结束标志，通知解码完成
    frame_queue.set_eof();

    // 释放分配的 AVFrame 内存
    av_frame_free(&frame);
}


