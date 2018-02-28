#ifndef QRPC_UTIL_EVENT_QUEUE_H
#define QRPC_UTIL_EVENT_QUEUE_H

#include <string>
#include <queue>
#include <event.h>
#include <pthread.h>
#include <tr1/functional>

namespace qrpc {

class Task;

class EvQueue {
public:
    explicit EvQueue(event_base *base);
    ~EvQueue();

    /* The event base associated with the queue */
    event_base* base() { return base_; }

    /**
     * Add task into tha tail of the queue,
     * and wake up the event.
     *
     * Returns true if success, false otherwise.
     */
    bool Push(Task *task);

    /**
     * Remove events from the queue,
     * and call the 'Quit' function of tasks.
     *
     * Call this when releasing the event queue.
     */
    void Clear();

    /** Stop the event queue */
    void Quit() { quit_ = true; }

private:
    static void OnEvent(int fd, short events, void *arg);

private:
    bool quit_;
    int fd_;
    event ev_;
    event_base *base_;

    pthread_mutex_t mutex_;
    std::queue<Task *> queue_;

private:
    /* No copying allowed */
    EvQueue(const EvQueue &);
    void operator=(const EvQueue &);
};

} // namespace qrpc

#endif /* QRPC_UTIL_EVENT_QUEUE_H */
