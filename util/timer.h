#ifndef QRPC_UTIL_TIMER_H
#define QRPC_UTIL_TIMER_H

#include <sys/time.h>
#include <assert.h>
#include <string.h>
#include <event.h>
#include <tr1/functional>

namespace qrpc {

/* millisecond timer event */
class Timer {
public:
    typedef std::tr1::function<void()> Handle;

    Timer(event_base *base, uint64_t msec, const Handle &handle);
    Timer();
    ~Timer();

    /* ugly */
    void Set(event_base *base, uint64_t msec, const Handle &handle);

    bool IsPending() {
        return (evtimer_pending(&ev_, NULL) != 0);
    }

    event_base* base() { return base_; }

    void SchedOneshot();
    void SchedPersist();
    void SchedCancel();

private:
    static void default_callback(int, short, void *);
    static void oneshot_callback(int , short, void *);
    static void persist_callback(int, short, void *);

private:
    Handle handle_;

    timeval tv_;
    event ev_;
    event_base *base_;

private:
    /* No copying allowed */
    Timer(const Timer &);
    void operator=(const Timer &);
};

} // namespace qrpc

#endif /* QRPC_UTIL_TIMER_H */
