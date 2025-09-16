#include "demuxer.h"
#include <iostream>

void demuxer(AVFormatContext* fmt_ctx, PacketQueue& video_queue, PacketQueue& audio_queue,int video_stream,int audio_stream) {
    AVPacket* pkt = av_packet_alloc();
    while(av_read_frame(fmt_ctx, pkt) >= 0) {
        if(pkt->stream_index == video_stream) {
            AVPacket* pkt_copy = av_packet_alloc();
            av_packet_ref(pkt_copy, pkt);
            video_queue.push(pkt_copy);
        } else if (pkt->stream_index == audio_stream) {
            AVPacket* pkt_copy = av_packet_alloc();
            av_packet_ref(pkt_copy, pkt);
            audio_queue.push(pkt_copy);
        }
        av_packet_unref(pkt);
    }
    video_queue.set_eof();
    audio_queue.set_eof();
    av_packet_free(&pkt);
}

