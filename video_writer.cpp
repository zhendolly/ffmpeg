#include "video_writer.h"
#include <fstream>
#include <queue>
#include <mutex>
#include <condition_variable>

extern "C" {
#include "libavcodec/avcodec.h"
}

extern std::queue<AVFrame*> video_frame_queue;
extern std::mutex video_mutex;
extern std::condition_variable video_cv;


void video_writer() {
    std::ofstream video_file("video_frames.yuv", std::ios::binary);
    if (!video_file.is_open()) {
//        std::cout << "无法打开输出文件" << std::endl;
        return;
    }

    while (true) {
        std::unique_lock<std::mutex> lock(video_mutex);
        video_cv.wait(lock, []{ return !video_frame_queue.empty(); });

        AVFrame* frame = video_frame_queue.front();
        video_frame_queue.pop();

        if (frame == nullptr) { // 结束标志
            break;
        }

        // 写入 Y 分量
        for (int j = 0; j < frame->height; ++j) {
            video_file.write(reinterpret_cast<char*>(frame->data[0] + j * frame->linesize[0]), frame->width);
        }

        // 写入 U 分量
        for (int j = 0; j < frame->height / 2; ++j) {
            video_file.write(reinterpret_cast<char*>(frame->data[1] + j * frame->linesize[1]), frame->width / 2);
        }

        // 写入 V 分量
        for (int j = 0; j < frame->height / 2; ++j) {
            video_file.write(reinterpret_cast<char*>(frame->data[2] + j * frame->linesize[2]), frame->width / 2);
        }

        av_frame_free(&frame);
    }

    video_file.close();
}