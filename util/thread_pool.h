#ifndef QRPC_UTIL_THREAD_POOL_H
#define QRPC_UTIL_THREAD_POOL_H

#include <vector>
#include <queue>
#include <string>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

namespace qrpc {

class Task;

class ThreadPool {
public:
    explicit ThreadPool(int threads, int timeout,
            const std::string name = "worker");
    ~ThreadPool();

    /**
     * Add task into tha tail of the queue,
     * and wake up an idle thread if need.
     *
     * Returns true if success, false otherwise.
     */
    bool Push(Task *task) {
        if (quit_) { return false; }

        pthread_mutex_lock(&mutex_);
        tasks_.push(task);
        if (idle_ > 0) {
            pthread_cond_signal(&cond_);
        }
        pthread_mutex_unlock(&mutex_);

        return true;
    }

private:
    std::string Name();
    static void* Main(void *arg);

private:
    bool quit_;
    int tid_;
    int threads_;
    std::string name_;

    pthread_cond_t cond_;
    pthread_mutex_t mutex_;
    std::queue<Task *> tasks_;

    int idle_;
    int idle_interval_;
    std::vector<pthread_t> workers_;

private:
    /* No copying allowed */
    ThreadPool(const ThreadPool &);
    void operator=(const ThreadPool &);
};

} // namespace qrpc

#endif /* QRPC_UTIL_THREAD_POOL_H */
