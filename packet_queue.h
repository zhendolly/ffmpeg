#ifndef PACKET_QUEUE_H
#define PACKET_QUEUE_H

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

struct PacketQueue {
    Queue<AVPacket*> queue;
    std::mutex mutex;
    std::condition_variable cond;
    bool eof = false;

    void push(AVPacket* pkt) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(pkt);
        cond.notify_one();
    }

    AVPacket* pop() {
        std::unique_lock<std::mutex> lock(mutex);
        while (queue.isEmpty() && !eof) {
            cond.wait(lock);
        }
        if (queue.isEmpty()) return nullptr;
        AVPacket* pkt = queue.peek();
        queue.pop();
        return pkt;
    }

    void set_eof() {
        std::lock_guard<std::mutex> lock(mutex);
        eof = true;
        cond.notify_all();
    }
};

#endif

