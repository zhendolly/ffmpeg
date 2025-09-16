#ifndef FRAME_QUEUE_H
#define FRAME_QUEUE_H

#include "Queue.h"
#include <mutex>
#include <condition_variable>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavcodec/avcodec.h"

#ifdef __cplusplus
}
#endif

struct FrameQueue {
    Queue<AVFrame*> queue;
    std::mutex mutex;
    std::condition_variable cond;
    bool eof = false;

    void push(AVFrame* frame) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(frame);
        cond.notify_one();
    }

    AVFrame* pop() {
        std::unique_lock<std::mutex> lock(mutex);
        while (queue.isEmpty() && !eof) {
            cond.wait(lock);
        }
        if (queue.isEmpty()) return nullptr;
        AVFrame* frame = queue.peek();
        queue.pop();
        return frame;
    }

    void set_eof() {
        std::lock_guard<std::mutex> lock(mutex);
        eof = true;
        cond.notify_all();
    }
};

#endif

