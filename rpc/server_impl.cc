#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <string>
#include <algorithm>

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/atomic.h"
#include "src/qrpc/util/random.h"
#include "src/qrpc/util/completion.h"
#include "src/qrpc/util/socket.h"
#include "src/qrpc/rpc/errno.h"
#include "src/qrpc/rpc/worker.h"
#include "src/qrpc/rpc/command.h"
#include "src/qrpc/rpc/message.pb.h"
#include "src/qrpc/rpc/builtin.h"
#include "src/qrpc/rpc/listener.h"
#include "src/qrpc/rpc/server.h"
#include "src/qrpc/rpc/server_impl.h"

using namespace std;
using namespace google::protobuf;

namespace qrpc {

ServerImpl::ServerImpl(const ServerOptions &options, event_base *base)
    : options_(options)
    , state_(kInit)
    , has_base_(base != NULL)
    , base_(base)
    , nxt_worker_(-1)
    , tid_(pthread_self())
{
   //pthread_rwlock_init(&service_lock_, NULL); 

    Register(&builtin_service_, kServerDoesntOwnService);
}

ServerImpl::~ServerImpl()
{
    DelServer();
    DelWorker();
    DelService();

    assert(listens_.empty() == true);
    assert(workers_.empty() == true);
    assert(services_.empty() == true);
}

bool ServerImpl::NewWorker()
{
    int num = options_.num_worker_thread;

    for (int i = 0; i < num; i++) {
        Worker *worker = new Worker(this);
        if (!worker) {
            LOG(FATAL) << "alloc worker thread failed";
        }

        /* use worker's event base */
        if (!has_base_) {
            base_ = worker->base();
        }

        workers_.push_back(worker);

        Thread *thread = worker->thread();
        if (each_workers_.find(thread->id()) != each_workers_.end()) {
            LOG(FATAL) << "invalid thread id: " << thread->id();
        }
        each_workers_[thread->id()] = worker;
    }

    return true;
}

void ServerImpl::DelWorker()
{
    for (size_t i = 0; i < workers_.size(); i++) {
        delete workers_[i];
    }

    workers_.clear();
}

bool ServerImpl::StartServer()
{
    for (size_t i = 0; i < endpoints_.size(); i++) {

        const string &host = endpoints_[i].first;
        int port = endpoints_[i].second;

        vector<sockinfo> sis;

        if (resolve_addr(host.c_str(), port, sis)) {
            LOG(ERROR) << "resolve network address failed: "
                << host << ":" << port;
            return false;
        }

        for (size_t i = 0; i < sis.size(); i++) {
            Listener *listener = new Listener(this);

            if (listener->Start(sis[i])) {
                listens_.push_back(listener);
            } else {
                LOG(ERROR) << "listen network address failed: "
                    << host << ":" << port;
                delete listener;
            }
        }
    }

    return !listens_.empty();
}

bool ServerImpl::NewServer()
{
    assert(base_ != NULL);

    /* use the user's event base */
    if (has_base_) {
        return StartServer();
    }

    /* use the worker's event base */
    Completion work(1);

    Listen cmd(this, workers_.back(), true, work);
    workers_.back()->Listen(&cmd);

    /* wait to add listen socket's event */
    work.Wait();

    return cmd.res_;
}

void ServerImpl::StopServer()
{
    for (size_t i = 0; i < listens_.size(); i++) {
        delete listens_[i];
    }

    listens_.clear();
}

void ServerImpl::DelServer()
{
    assert(base_ != NULL);

    /* use the user's event base */
    if (has_base_) {
        return StopServer();
    }

    /* use the worker's event base */
    Completion work(1);

    Listen cmd(this, workers_.back(), false, work);
    workers_.back()->Listen(&cmd);

    /* wait to remove listen socket's event */
    work.Wait();
}

bool ServerImpl::Dispatch(int sfd, std::string &local, std::string &remote)
{
    int nxt = (++nxt_worker_);
    if (nxt < 0) nxt = -nxt;
    int idx = nxt % options_.num_worker_thread;
    Worker *worker = workers_[idx];

    Link *link = new Link(worker, sfd, local, remote);
    if (!link) {
        LOG(ERROR) << "alloc link object failed";
        return false;
    }

    worker->Link(link);

    return true;
}

const string& ServerImpl::state() const
{
    static string state_msg[] = {
        /* kInit */ string("initialized state"),
        /* kRun  */ string("running state"),
        /* kExit */ string("exited state"),
    };

    static string what_is_the_fuck = string("Are you fucking kidding me");

    switch (state_) {
    case kInit:
    case kRun:
    case kExit:
        return state_msg[state_];
    default:
        return what_is_the_fuck;
    }
}

int ServerImpl::Add(const string &host, int port)
{
    if (pthread_self() != tid_) {
        LOG(ERROR) << "run in the alloc thread context";
        return kErrCtx;
    }

    if (host.empty()) {
        LOG(ERROR) << "host address is empty";
        return kErrParam;
    }

    if (port <= 0) {
        LOG(ERROR) << "network port is invalid";
        return kErrParam;
    }

    if (state_ != kInit) {
        LOG(ERROR) << "the server is in: " << state();
        return kError;
    }

    pair<string, int> endpoint(host, port);

    if (find(endpoints_.begin(), endpoints_.end(), endpoint)
        != endpoints_.end()) {
        LOG(ERROR) << "there's the endpoint: " << host << ":" << port;
        return kErrParam;
    }

    endpoints_.push_back(endpoint);

    return kOk;
}

int ServerImpl::Start()
{
    if (pthread_self() != tid_) {
        LOG(ERROR) << "run in the alloc thread context";
        return kErrCtx;
    }

    if (state_ != kInit) {
        LOG(ERROR) << "the server is in: " << state();
        return kError;
    }

    if (endpoints_.empty()) {
        LOG(ERROR) << "there's any transport endpoint";
        return kError;
    }

    if (!NewWorker()) {
        LOG(ERROR) << "create worker thread failed";
        return kError;
    }

    if (!NewServer()) {
        LOG(ERROR) << "create listen socket failed";
        return kError;
    }

    state_ = kRun;
    return 0;
}

int ServerImpl::Stop()
{
    if (pthread_self() != tid_) {
        LOG(ERROR) << "run in the alloc thread context";
        return kErrCtx;
    }

    //if (state_ != kRun) {
    //    LOG(ERROR) << "the server is in: " << state();
    //    return kError;
    //}

    DelServer();
    DelWorker();
    DelService();

    state_ = kExit;
    return 0;
}

int ServerImpl::Register(Service *service, ServiceOwnership ownership)
{
    if (pthread_self() != tid_) {
        LOG(ERROR) << "run in the alloc thread context";
        return kErrCtx;
    }

    if (state_ != kInit) {
        LOG(ERROR) << "the server is in: " << state();
        return kError;
    }

    if (unlikely(!service)) {
        LOG(ERROR) << "invalid service param";
        return kErrParam;
    }

    const ServiceDescriptor *desc = service->GetDescriptor();
    const string &fname = desc->full_name();

    bool res = true;
    pair<map<string, Service *>::iterator, bool> res1;
    pair<map<string, ServiceOwnership>::iterator, bool> res2;

    //pthread_rwlock_wrlock(&service_lock_);

    res1 = services_.insert(pair<string, Service *>(fname, service));
    if (!res1.second) {
        res = false;
        goto unlock;
    }

    if (ownership != kServerOwnsService) {
        goto unlock;
    }

    res2 = ownership_.insert(pair<string, ServiceOwnership>(fname, ownership));
    if (!res2.second) {
        res = false;
        services_.erase(res1.first);
    }

unlock:
    //pthread_rwlock_unlock(&service_lock_);

    return (res ? kOk : kErrHasSrv);
}

int ServerImpl::Unregister(string service_full_name)
{
    if (pthread_self() != tid_) {
        LOG(ERROR) << "run in the alloc thread context";
        return kErrCtx;
    }

    if (state_ == kRun) {
        LOG(ERROR) << "the server is in: " << state();
        return kError;
    }

    if (service_full_name.empty()) {
        LOG(ERROR) << "invalid service full name";
        return kErrParam;
    }

    map<string, Service *>::iterator res1;
    map<string, ServiceOwnership>::iterator res2;

    Service *service = NULL;
    ServiceOwnership ownership = kServerDoesntOwnService;

    //pthread_rwlock_wrlock(&service_lock_);

    res1 = services_.find(service_full_name);
    if (res1 != services_.end()) {
        service = res1->second;
        services_.erase(res1);
    }

    res2 = ownership_.find(service_full_name);
    if (res2 != ownership_.end()) {
        ownership = res2->second;
        ownership_.erase(res2);
    }

    //pthread_rwlock_unlock(&service_lock_);

    if (!service) {
        LOG(ERROR) << "not registered service: " << service_full_name;
        return kErrNotSrv;
    }

    if (ownership == kServerOwnsService) {
        delete service;
    }

    return 0;
}

int ServerImpl::Unregister(Service *srvptr)
{
    if (pthread_self() != tid_) {
        LOG(ERROR) << "run in the alloc thread context";
        return kErrCtx;
    }

    if (state_ == kRun) {
        LOG(ERROR) << "the server is in: " << state();
        return kError;
    }

    if (unlikely(!srvptr)) {
        LOG(ERROR) << "invalid service param";
        return kErrParam;
    }

    const ServiceDescriptor *desc = srvptr->GetDescriptor();
    const string &fname = desc->full_name();

    map<string, Service *>::iterator res1;
    map<string, ServiceOwnership>::iterator res2;

    Service *service = NULL;
    ServiceOwnership ownership = kServerDoesntOwnService;

    //pthread_rwlock_wrlock(&service_lock_);

    res1 = services_.find(fname);
    if (res1 != services_.end()) {
        service = res1->second;
        services_.erase(res1);
    }

    res2 = ownership_.find(fname);
    if (res2 != ownership_.end()) {
        ownership = res2->second;
        ownership_.erase(res2);
    }

    //pthread_rwlock_unlock(&service_lock_);

    if (!service) {
        LOG(ERROR) << "not registered service: " << fname;
        return kErrNotSrv;
    }

    if (ownership == kServerOwnsService) {
        delete service;
    }

    return 0;
}

void ServerImpl::DelService()
{
    //pthread_rwlock_wrlock(&service_lock_);

    for (map<string, Service *>::iterator it = services_.begin();
         it != services_.end(); it++) {
        string fname = it->first;
        Service *service = it->second;

        map<string, ServiceOwnership>::iterator ite = ownership_.find(fname);
        if (ite == ownership_.end()) {
            continue;
        }
        if (ite->second != kServerOwnsService) {
            continue;
        }

        delete service;
    }

    services_.clear();
    ownership_.clear();

    //pthread_rwlock_unlock(&service_lock_);
}

} // namespace qrpc
