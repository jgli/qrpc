#ifndef QRPC_RPC_CONTROLLER_SERVER_H
#define QRPC_RPC_CONTROLLER_SERVER_H

#include <stdint.h>
#include <event.h>
#include <pthread.h>
#include <string>

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "src/qrpc/util/timer.h"
#include "src/qrpc/util/compiler.h"

namespace qrpc {

class Controller;
class ServerMessage;

class ServerController : public Controller {
public:
    explicit ServerController(ServerMessage *srv_msg);
    virtual ~ServerController();

    /* Client & Server shared methods */
    virtual std::string LocalAddress() const;
    virtual std::string RemoteAddress() const;

    /* Client-side methods */
    virtual void Reset();
    virtual bool Failed() const;
    virtual std::string ErrorText() const;
    virtual void StartCancel();

    /* Server-side methods */
    virtual void SetFailed(const std::string &reason);
    virtual bool IsCanceled() const;
    virtual void NotifyOnCancel(google::protobuf::Closure *callback);

public:
    inline void CancelRequest() {
        assert(cancel_ == false);
        cancel_ = true;
        if (closure_) { closure_->Run(); closure_ = NULL; }
    }

    inline void FinishRequest() {
        if (cancel_) { return; }
        if (closure_) { closure_->Run(); closure_ = NULL; }
    }

    pthread_t thread_context()      const { return tid_;        }
    uint32_t code()                 const { return code_;       }
    const std::string& error_text() const { return error_text_; }

    void SetResponseCode(uint32_t code) { code_ = code; }
    void SetResponseError(const std::string &error) { error_text_ = error; }

private:
    pthread_t tid_;
    ServerMessage *srv_msg_;

    uint32_t code_;
    std::string error_text_;

    bool cancel_;
    google::protobuf::Closure *closure_;
};

} // namespace qrpc

#endif /* QRPC_RPC_CONTROLLER_SERVER_H */
