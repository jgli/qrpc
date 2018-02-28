#include <stdlib.h>
#include <assert.h>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/timer.h"
#include "src/qrpc/util/compiler.h"

namespace qrpc {

Timer::Timer(event_base *base, uint64_t msec, const Handle &handle)
{
    Set(base, msec, handle);
}

Timer::Timer()
    : base_(NULL)
{

}

Timer::~Timer()
{

}

void Timer::Set(event_base *base, uint64_t msec, const Handle &handle)
{
    assert(msec > 0);
    assert(base != NULL);

    base_ = base;
    handle_ = handle;

    if (msec > 1000) {
        tv_.tv_sec = msec / 1000;
        tv_.tv_usec = (msec % 1000) * 1000;
    } else {
        tv_.tv_sec = 0;
        tv_.tv_usec = msec * 1000;
    }

    evtimer_set(&ev_, default_callback, NULL);
}

void Timer::default_callback(int fd, short ev, void *arg)
{
    LOG(FATAL) << "timer is in negative state";
}

void Timer::oneshot_callback(int fd, short ev, void *arg)
{
    Timer *t = (Timer *)arg;

    evtimer_set(&t->ev_, default_callback, NULL);
    t->handle_();
}

void Timer::persist_callback(int fd, short ev, void *arg)
{
    Timer *t = (Timer *)arg;

    t->handle_();
}

void Timer::SchedOneshot()
{
    event_callback_fn cb = event_get_callback(&ev_);
    if (unlikely(cb != default_callback)) {
        LOG(FATAL) << "timer is in running state";
    }

    event_set(&ev_, -1, 0, oneshot_callback, this);
    event_base_set(base_, &ev_);

    int rc = evtimer_add(&ev_, &tv_);
    if (unlikely(rc != 0)) {
        LOG(FATAL) << "add timer failed!!!";
    }
}

void Timer::SchedPersist()
{
    event_callback_fn cb = event_get_callback(&ev_);
    if (unlikely(cb != default_callback)) {
        LOG(FATAL) << "timer is in running state";
    }

    event_set(&ev_, -1, EV_PERSIST, persist_callback, this);
    event_base_set(base_, &ev_);

    int rc = evtimer_add(&ev_, &tv_);
    if (unlikely(rc != 0)) {
        LOG(FATAL) << "add timer failed!!!";
    }
}

void Timer::SchedCancel()
{
    evtimer_del(&ev_);
    evtimer_set(&ev_, default_callback, NULL);
}

} // namespace qrpc
