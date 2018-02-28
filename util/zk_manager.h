#ifndef QRPC_UTIL_ZK_MANAGER_H
#define QRPC_UTIL_ZK_MANAGER_H

#include <event.h>
#include <string>
#include <tr1/functional>
#include <zookeeper/zookeeper.h>

namespace qrpc {

struct ZkConfig {
    /* DEBUG, INFO, WARN, ERROR */
    std::string verbose_;
    std::string hosts_;
    uint64_t    timeout_ms_;
};

class ZkManager {
public:
    explicit ZkManager(event_base *base, const ZkConfig &conf);
    ~ZkManager();

    bool Open();
    void Close();

    bool Create(const std::string &path, const std::string &value,
            string_completion_t completion, const void *arg, int flags = 0);

    bool Get(const std::string &path,
            data_completion_t completion, const void *arg);

    bool Set(const std::string &path, const std::string &value,
            stat_completion_t completion, const void *arg);

    bool Del(const std::string &path,
            void_completion_t completion, const void *arg);

    bool GetChildren(const std::string &path,
            strings_completion_t completion, const void *arg);

    bool Multi(int count, const zoo_op_t *ops, zoo_op_result_t *results,
            void_completion_t completion, const void *arg);

private:
    void ReOpen();
    void Keepalive();

    static void HandleWatcher(zhandle_t *zh, int type,
            int state, const char *path, void *ctx);
    static void HandleAlive(int fd, short events, void *arg);

private:
    zhandle_t   *zh_;

    event_base  *base_;
    event       ev_;
    ZkConfig    conf_;
};

} // namespace qrpc

#endif /* QRPC_UTIL_ZK_MANAGER_H */
