#include <sys/prctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/completion.h"
#include "src/qrpc/util/task.h"
#include "src/qrpc/util/thread_pool.h"

using namespace std;

namespace qrpc {

ThreadPool::ThreadPool(int threads, int timeout, const std::string name)
    : quit_(false)
    , tid_(0)
    , threads_(threads)
    , name_(name)
    , idle_(0)
    , idle_interval_(timeout)
{
    pthread_cond_init(&cond_, NULL);
    pthread_mutex_init(&mutex_, NULL);

    if (name.empty()) {
        name_.assign("worker");
    }
    if (threads < 1) {
        LOG(FATAL) << "invalid parameters @threads: " << threads;
    }
    if (timeout < 1) {
        LOG(FATAL) << "invalid parameters @timeout: " << timeout;
    }

    Completion work(threads);

    struct {
        ThreadPool *tp;
        Completion *work;
    } arg = {this, &work};

    for (int i = 0; i < threads; ++i) {
        pthread_t tid;
        if (pthread_create(&tid, NULL, Main, &arg)) {
            LOG(FATAL) << "create thread failed!!!";
        }
        workers_.push_back(tid);
    }

    work.Wait();
}

ThreadPool::~ThreadPool()
{
    pthread_mutex_lock(&mutex_);
    quit_ = true;
    if (idle_ > 0) {
        pthread_cond_broadcast(&cond_);
    }
    pthread_mutex_unlock(&mutex_);

    for (size_t i = 0; i < workers_.size(); ++i) {
        pthread_join(workers_[i], NULL);
    }

    while (!tasks_.empty()) {
        tasks_.front()->Quit();
        tasks_.pop();
    }
}

string ThreadPool::Name()
{
    char tmp[30] = {0};
    int id = __sync_add_and_fetch(&tid_, 1);

    if (threads_ == 1) {
        snprintf(tmp, 30, "[%s]", name_.c_str());
    } else {
        snprintf(tmp, 30, "[%s/%02d]", name_.c_str(), id);
    }
    
    return string(tmp);
}

void* ThreadPool::Main(void *arg)
{
    struct father {
        ThreadPool *tp;
        Completion *work;
    };

    father *f = (father *)arg;
    ThreadPool *tp = f->tp;
    f->work->Signal();

    string name = tp->Name();
    prctl(PR_SET_NAME, name.c_str(), 0, 0, 0);

    for (; ;) {
        pthread_mutex_lock(&tp->mutex_);

        if (tp->quit_) {
            pthread_mutex_unlock(&tp->mutex_);
            return NULL;
        }

        while (tp->tasks_.empty()) {
            struct timespec timeout;
            timeout.tv_sec = time(NULL) + tp->idle_interval_;
            timeout.tv_nsec = 0;

            tp->idle_++;
            pthread_cond_timedwait(&tp->cond_, &tp->mutex_, &timeout);
            tp->idle_--;

            if (tp->quit_) {
                pthread_mutex_unlock(&tp->mutex_);
                return NULL;
            }
        }

        Task *task = tp->tasks_.front();
        tp->tasks_.pop();

        pthread_mutex_unlock(&tp->mutex_);

        (*task)();
    }

    return NULL;
}

} // namespace qrpc
