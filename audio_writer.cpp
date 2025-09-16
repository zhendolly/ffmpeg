#include "audio_writer.h"
#include <iostream>
#include <fstream>
#include <queue>
#include <mutex>
#include <condition_variable>

extern "C" {
#include "libavcodec/avcodec.h"
}

extern std::queue<AVFrame*> audio_frame_queue;
extern std::mutex audio_mutex;
extern std::condition_variable audio_cv;

void audio_writer() {
    std::ofstream audio_file("audio_frames.pcm", std::ios::binary);
    if (!audio_file.is_open()) {
        std::cerr << "无法打开音频输出文件" << std::endl;
        return;
    }

    while (true) {
        AVFrame* frame = nullptr;
        {
            std::unique_lock<std::mutex> lock(audio_mutex);
            std::cout << "等待音频帧，当前队列大小: " << audio_frame_queue.size() << std::endl;
            audio_cv.wait(lock, []{ return !audio_frame_queue.empty(); });
            frame = audio_frame_queue.front();
            audio_frame_queue.pop();
        }

        // 检查结束信号
        if (frame == nullptr) {
            std::cout << "接收到音频结束信号" << std::endl;
            break;
        }

        // 计算样本尺寸
        const int sample_size = av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format));
        if (sample_size <= 0) {
            std::cerr << "无效的音频样本格式" << std::endl;
            av_frame_free(&frame);
            continue;
        }

        // 写入交错格式的PCM数据
        std::cout << frame->nb_samples << std::endl;
        std::cout << frame->channels << std::endl;
        for (int i = 0; i < frame->nb_samples; i++) {
            for (int ch = 0; ch < frame->channels; ch++) {
                const uint8_t* data = frame->data[ch] + i * sample_size;
                //std::cout << 1357 << std::endl;
                audio_file.write(reinterpret_cast<const char*>(data), sample_size);
            }
        }

        av_frame_free(&frame);
    }

    std::cout << "音频写入完成" << std::endl;
}
