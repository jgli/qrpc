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
#include "src/qrpc/util/completion.h"
#include "src/qrpc/util/event_queue.h"
#include "src/qrpc/util/thread.h"

using namespace std;

namespace qrpc {

Thread::Thread(std::string name,
        const Init &init, const Exit &exit)
    : evq_(NULL)
    , fd_(-1)
    , base_(NULL)
    , init_(init)
    , exit_(exit)
    , id_(0)
    , name_(name)
{
    Completion work(1);
    pthread_t tid;

    struct {
        Thread *thread;
        Completion *work;
    } args = { this, &work};

    if (pthread_create(&tid, NULL, Main, &args)
        != 0) {
        LOG(FATAL) << "create thread failed!!!";
    }
    work.Wait();
}

Thread::~Thread()
{
    assert(evq_ != NULL);
    assert(base_ != NULL);

    /* stop event queue */
    evq_->Quit();

    /* notify the break event */
    for (; ;) {
        uint64_t u = 1;
        int ret = write(fd_, &u, sizeof(u));
        if (ret == sizeof(u)) { break; }
    }

    /* wait exiting the thread */
    pthread_join(id_, NULL);
}

void Thread::Break(int fd, short events, void *arg)
{
    Thread *me = (Thread *)arg;

	uint64_t u;
    read(me->fd_, &u, sizeof(u));

    event_base_loopbreak(me->base_);
}

void Thread::InitEnviroment()
{
    /* set thread id */
    id_ = pthread_self();

    /* set thread name */
    if (!name_.empty()) {
        const char *name = name_.c_str();
        prctl(PR_SET_NAME, name, 0, 0, 0);
    }

    /* alloc event base */
    base_ = event_base_new();
    if (!base_) {
        LOG(FATAL) << "alloc event base failed";
    }

    /* set the break event */
    int n = 0;

    fd_ = syscall(SYS_eventfd, 0);
	if (fd_ < 0) {
        LOG(FATAL) << "syscall failed: " << errno;
	}
	if (ioctl(fd_, FIONBIO, &n)) {
        LOG(FATAL) << "ioctl failed: " << errno;
	}

    event_set(&ev_, fd_,
              EV_READ | EV_PERSIST, Break, this);
    event_base_set(base_, &ev_);
    if (event_add(&ev_, NULL)) {
        LOG(FATAL) << "event_add failed";
    }

    /* alloc event queue */
    evq_ = new EvQueue(base_);
    if (!evq_) {
        LOG(FATAL) << "alloc event queue failed";
    }

    /* call user's initialize callback */
    init_(this);
}

void Thread::ExitEnviroment()
{
    /* cancel pending jobs */
    evq_->Clear();

    /* call user's destroy callback */
    exit_(this);

    /* delete local variable */
    delete evq_;
    event_del(&ev_);
    close(fd_);
    event_base_free(base_);
}

void* Thread::Main(void *arg)
{
    struct father {
        Thread *thread;
        Completion *work;
    };

    father *f = (father *)arg;
    Thread *me = f->thread;

    /* create enviroment */
    me->InitEnviroment();

    /* finish creating thread */
    f->work->Signal();

    /* main loop */
    event_base_loop(me->base_, 0);

    /* destroy enviroment */
    me->ExitEnviroment();

    return NULL;
}

} // namespace qrpc
