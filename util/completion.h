#ifndef QRPC_UTIL_COMPLETION_H
#define QRPC_UTIL_COMPLETION_H

#include <asm/errno.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>

namespace qrpc {

class Completion {
public:
    explicit Completion(uint32_t events)
        : waits_(events)
        , signals_(0) {
        pthread_cond_init(&cond_, NULL);
        pthread_mutex_init(&mutex_, NULL);
    }

    ~Completion() {
        pthread_cond_destroy(&cond_);
        pthread_mutex_destroy(&mutex_);
    }

    /**
     * Waits for completion of a task.
     *
     * This waits to be signaled for completion of a specific task.
     * It is NOT interruptible and there is no timeout.
     */
    void Wait() {
        pthread_mutex_lock(&mutex_);
        while (signals_ < waits_) {
            pthread_cond_wait(&cond_, &mutex_);
        }
        pthread_mutex_unlock(&mutex_);
    }

    /**
     * Waits for completion of a task (w/timeout).
     *
     * This waits for either a completion of a specific task to be signaled
     * or for a specified timeout to expire. The timeout is in seconds. 
     * It is NOT interruptible.
     *
     * Return true if there are no waiters,
     * Return false if there are waiters (timeout).
     */
    bool Wait(uint32_t timeout) {
        pthread_mutex_lock(&mutex_);
        while (signals_ < waits_) {
            struct timespec t;
            t.tv_sec = time(NULL) + timeout;
            t.tv_nsec = 0;
            int status = pthread_cond_timedwait(&cond_, &mutex_, &t);
            if (status == ETIMEDOUT) {
                pthread_mutex_unlock(&mutex_);
                return false;
            }
        }
        pthread_mutex_unlock(&mutex_);
        return true;
    }
    
    /**
     * Test to see if a completion has any waiters.
     *
     * Return true if there are no waiters,
     * Return false if there are waiters (in progress).
     */
    bool Done() {
        pthread_mutex_lock(&mutex_);
        bool done = (signals_ == waits_);
        pthread_mutex_unlock(&mutex_);
        return done;
    }

    /** 
     * Signal a single thread waiting on this completion.
     *
     * This will wake up a single thread waiting on this completion
     * if there are no events (in process).
     */
    void Signal() {
        pthread_mutex_lock(&mutex_);
        if (++signals_ == waits_) {
            pthread_cond_signal(&cond_);
        }
        pthread_mutex_unlock(&mutex_);
    }

    /**
     * Signal all threads waiting on this completion.
     *
     * This will wake up all threads waiting on this
     * particular completion if there are no events (in process).
     */
    void SignalAll() {
        pthread_mutex_lock(&mutex_);
        if (++signals_ == waits_) {
            pthread_cond_broadcast(&cond_);
        }
        pthread_mutex_unlock(&mutex_);
    }

private:
    uint32_t waits_;
    uint32_t signals_;

    pthread_cond_t cond_;
    pthread_mutex_t mutex_;

private:
    /* No copying allowed */
    Completion(const Completion &);
    void operator=(const Completion &);
};

} // namespace qrpc

#endif /* QRPC_UTIL_COMPLETION_H */
