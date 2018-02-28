#ifndef QRPC_UTIL_TASK_H
#define QRPC_UTIL_TASK_H

namespace qrpc {

class Task {
public:
    Task() { }
    virtual ~Task() { }
    virtual void Quit() = 0;
    virtual void operator()() = 0;
};

} // namespace qrpc

#endif /* QRPC_UTIL_TASK_H */
