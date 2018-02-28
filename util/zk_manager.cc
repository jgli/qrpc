#include <stdlib.h>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/timer.h"
#include "src/qrpc/util/zk_manager.h"

using namespace std;

namespace qrpc {

ZkManager::ZkManager(event_base *base, const ZkConfig &conf)
    : zh_(NULL)
    , base_(base)
    , conf_(conf)
{

}

ZkManager::~ZkManager()
{

}

void ZkManager::HandleWatcher(zhandle_t *zh, int type, int state,
                              const char *path, void *ctx)
{
    ZkManager *me = (ZkManager *)ctx;

    if (type != ZOO_SESSION_EVENT) {
        return;
    }

    if (state == ZOO_EXPIRED_SESSION_STATE) {
        //zookeeper_close(zh);
        //me->zh_ = NULL;
        LOG(ERROR) << "zookeeper session expired!!!";
    } else if (state == ZOO_AUTH_FAILED_STATE) {
        zookeeper_close(zh);
        me->zh_ = NULL;
        LOG(FATAL) << "zookeeper auth failed!!!";
    } else if (state == ZOO_CONNECTING_STATE) {
        LOG(INFO) << "connecting to zookeeper...";
    } else if (state == ZOO_ASSOCIATING_STATE) {
        LOG(INFO) << "associating to zookeeper...";
    } else if (state == ZOO_CONNECTED_STATE) {
        LOG(INFO) << "has connected to zookeeper.";
    } else {
        LOG(FATAL) << "receive invalid zookeeper's state!!!";
    }
}

bool ZkManager::Open()
{
    if (zh_ != NULL) {
        LOG(ERROR) << "zookeeper already exist!!!";
        return false;
    }

    if (conf_.verbose_ == "ERROR") {
        zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);
    } else if (conf_.verbose_ == "WARN") {
        zoo_set_debug_level(ZOO_LOG_LEVEL_WARN);
    } else if (conf_.verbose_ == "INFO") {
        zoo_set_debug_level(ZOO_LOG_LEVEL_INFO);
    } else if (conf_.verbose_ == "DEBUG") {
        zoo_set_debug_level(ZOO_LOG_LEVEL_DEBUG);
    } else {
        zoo_set_debug_level(ZOO_LOG_LEVEL_INFO);
    }

    zh_ = zookeeper_init(conf_.hosts_.c_str(),
                         HandleWatcher,
                         conf_.timeout_ms_, NULL, this, 0);
    if (zh_ != NULL) {
        Keepalive();
        return true;
    } else {
        LOG(ERROR) << "zookeeper_init() failed!!!";
        return false;
    }
}

void ZkManager::ReOpen()
{
    if (zh_ != NULL) {
        zookeeper_close(zh_);
        zh_ = NULL;
    }

    zh_ = zookeeper_init(conf_.hosts_.c_str(),
                         HandleWatcher,
                         conf_.timeout_ms_, NULL, this, 0);
    if (!zh_) {
        LOG(FATAL) << "call zookeeper_init() failed!!!";
    }

    Keepalive();
}

void ZkManager::Close()
{

}

void ZkManager::Keepalive()
{
    int rc, fd;
    int events = 0;
    int interest = 0;
    struct timeval tv = { 1, 0 };

    rc = zookeeper_interest(zh_, &fd, &interest, &tv);
    if (rc != ZOK) {
        LOG(ERROR) << "zookeeper_interest() failed"
            << ", msg: " << zerror(rc);
        
        if (is_unrecoverable(zh_)) {
            LOG(INFO) << "close zookeeper!!!";

            zookeeper_close(zh_);
            zh_ = NULL;

            return ReOpen();
        }
    }

    if (interest & ZOOKEEPER_READ) {
        events |= EV_READ;
    }
    if (interest & ZOOKEEPER_WRITE) {
        events |= EV_WRITE;
    }

    event_set(&ev_, fd, events, HandleAlive, this);
    event_base_set(base_, &ev_);

    if (event_add(&ev_, &tv)) {
        LOG(FATAL) << "event_add() failed!!!";
    }
}

void ZkManager::HandleAlive(int fd, short events, void *arg)
{
    ZkManager *me = (ZkManager *)arg;
    int rc;
    int interest = 0;

    if (!me->zh_) {
        LOG(ERROR) << "zookeeper closed already!!!";
        return;
    }

    if (events & EV_READ) {
        interest |= ZOOKEEPER_READ;
    }
    if (events & EV_WRITE) {
        interest |= ZOOKEEPER_WRITE;
    }

    rc = zookeeper_process(me->zh_, interest);
    if (rc != ZOK) {
        if (rc != ZNOTHING) {
            LOG(ERROR) << "zookeeper_process() failed"
                << ", msg: " << zerror(rc);
        }

        if (is_unrecoverable(me->zh_)) {
            LOG(INFO) << "close zookeeper!!!";

            zookeeper_close(me->zh_);
            me->zh_ = NULL;

            return me->ReOpen();
        }
    }

    me->Keepalive();
}

bool ZkManager::Create(const string &path, const string &value,
                       string_completion_t completion, const void *arg, int flags)
{
    if (zh_ == NULL) {
        return false;
    }

    int res = zoo_acreate(zh_, path.c_str(),
                          value.c_str(), value.size(),
                          &ZOO_OPEN_ACL_UNSAFE, flags,
                          completion, arg);
    return (res == ZOK);
}

bool ZkManager::GetChildren(const string &path,
                            strings_completion_t completion, const void *arg)
{
    if (zh_ == NULL) {
        return false;
    }

    int res = zoo_aget_children(zh_, path.c_str(), 0,
                                completion, arg);
    return (res == ZOK);

}

bool ZkManager::Get(const string &path,
                    data_completion_t completion, const void *arg)
{
    if (zh_ == NULL) {
        return false;
    }

    int res = zoo_aget(zh_, path.c_str(), 0, completion, arg);
    return (res == ZOK);
}

bool ZkManager::Set(const string &path, const string &value,
                    stat_completion_t completion, const void *arg)
{
    if (zh_ == NULL) {
        return false;
    }

    int res = zoo_aset(zh_, path.c_str(), value.c_str(), value.size(),
                       -1, completion, arg);
    return (res == ZOK);
}

bool ZkManager::Del(const string &path,
                    void_completion_t completion, const void *arg)
{
    if (zh_ == NULL) {
        return false;
    }

    int res = zoo_adelete(zh_, path.c_str(), -1, completion, arg);
    return (res == ZOK);
}

bool ZkManager::Multi(int count, const zoo_op_t *ops, zoo_op_result_t *results,
        void_completion_t completion, const void *arg)
{
    if (zh_ == NULL) {
        return false;
    }

    int res = zoo_amulti(zh_, count, ops, results, completion, arg);
    return (res == ZOK);
}

} // namespace qrpc
