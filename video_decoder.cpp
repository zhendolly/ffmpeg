#include "video_decoder.h"

void video_decoder(AVCodecContext* codec_ctx, PacketQueue& packet_queue, FrameQueue& frame_queue) {
    AVFrame* frame = av_frame_alloc();

    while(true) {
        AVPacket* pkt = packet_queue.pop();
        if(!pkt) break;

        int ret = avcodec_send_packet(codec_ctx, pkt);
        av_packet_free(&pkt);

        if(ret < 0) break;

        while(ret >= 0) {
            ret = avcodec_receive_frame(codec_ctx, frame);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;

            AVFrame* frame_copy = av_frame_alloc();
            av_frame_ref(frame_copy, frame);
            frame_queue.push(frame_copy);
        }
    }

    frame_queue.set_eof();
    av_frame_free(&frame);
}
