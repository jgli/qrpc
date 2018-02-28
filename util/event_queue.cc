#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/compiler.h"
#include "src/qrpc/util/task.h"
#include "src/qrpc/util/event_queue.h"

using namespace std;

namespace qrpc {

EvQueue::EvQueue(event_base *base)
    : quit_(false)
    , fd_(-1)
{
    int n = 0;

    fd_ = syscall(SYS_eventfd, 0);
	if (fd_ < 0) {
        LOG(FATAL) << "syscall failed: " << errno;
	}
	if (ioctl(fd_, FIONBIO, &n)) {
        LOG(FATAL) << "ioctl failed: " << errno;
	}

    event_set(&ev_, fd_,
              EV_READ | EV_PERSIST, OnEvent, this);
    event_base_set(base, &ev_);
    if (event_add(&ev_, NULL)) {
        LOG(FATAL) << "event_add failed";
    }

    pthread_mutex_init(&mutex_, NULL);
}

EvQueue::~EvQueue()
{
    quit_ = true;
    Clear();
    event_del(&ev_);
    close(fd_);
}

bool EvQueue::Push(Task *task)
{
    if (unlikely(quit_)) {
        return false;
    }

    pthread_mutex_lock(&mutex_);
    bool empty = queue_.empty();
    queue_.push(task);
    pthread_mutex_unlock(&mutex_);

    while (empty) {
        uint64_t u = 1;
        int ret = write(fd_, &u, sizeof(u));
        if (ret == sizeof(u)) { break; }
    }

    return true;
}

void EvQueue::Clear()
{
    for (; ;) {
        pthread_mutex_lock(&mutex_);
        if (queue_.empty()) {
            pthread_mutex_unlock(&mutex_);
            break;
        }
        Task *task = queue_.front();
        queue_.pop();
        pthread_mutex_unlock(&mutex_);

        task->Quit();
    }
}

void EvQueue::OnEvent(int fd, short events, void *arg)
{
    EvQueue *me = (EvQueue *)arg;

	uint64_t u;
    read(me->fd_, &u, sizeof(u));

    while (!me->quit_) {
        pthread_mutex_lock(&me->mutex_);
        if (me->queue_.empty()) {
            pthread_mutex_unlock(&me->mutex_);
            break;
        }
        Task *task = me->queue_.front();
        me->queue_.pop();
        pthread_mutex_unlock(&me->mutex_);

        (*task)();
    }
}

} // namespace qrpc
