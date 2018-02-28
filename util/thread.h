#ifndef QRPC_UTIL_THREAD_H
#define QRPC_UTIL_THREAD_H

#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <event.h>
#include <string>
#include <tr1/functional>

namespace qrpc {

class EvQueue;

class Thread {
public:
    /** The callback function of init thread */
    typedef std::tr1::function<void(Thread *thr)> Init;

    /** The callback function of exit thread */
    typedef std::tr1::function<void(Thread *thr)> Exit;

    /**
     * A thread based on event base and queue.
     *
     * @name : thread's name
     * @init : create user's environment
     * @exit : destroy user's enviroment
     *
     * Create and startup the thread
     * User shouldn't break the event base loop before delete it.
     */
    explicit Thread(std::string name,
            const Init &init, const Exit &exit);

    /**
     * Release the thread.
     *
     * Break the event loop and cancel all pending tasks.
     */
    ~Thread();

    /** the id of this thread */
    pthread_t id() const { return id_; }

    /** the name of this thread */
    std::string name() const { return name_; }

    /** the event base of this thread */
    event_base* base() { return base_; }

    /** the event queue of this thread */
    EvQueue* ev_queue() { return evq_; }

private:
    /* init thread local variable */
    void InitEnviroment();

    /* exit thread local variable */
    void ExitEnviroment();

    /* the main routine of this thread */
    static void* Main(void *arg);

    /* the callback routine of exit event */
    static void Break(int fd, short events, void *arg);

protected:
    /* event queue of the thread */
    EvQueue *evq_;

    /* exit event */
    int fd_;
    event ev_;

    /* event base of the thread */
    event_base *base_;

    /* callback of thread */
    Init init_;
    Exit exit_;

    /* thread info */
    pthread_t id_;
    std::string name_;

private:
    /* No copying allowed */
    Thread(const Thread &);
    void operator=(const Thread &);
};

} // namespace qrpc

#endif /* QRPC_UTIL_THREAD_H */
