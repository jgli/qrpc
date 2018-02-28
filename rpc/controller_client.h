#ifndef QRPC_RPC_CONTROLLER_CLIENT_H
#define QRPC_RPC_CONTROLLER_CLIENT_H

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
class ClientMessage;

class ClientController : public Controller {
public:
    explicit ClientController(const ControllerOptions &options);
    virtual ~ClientController();

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
    void SetOwnership(ClientMessage *message) {
        assert(!client_message_);
        tid_ = pthread_self();
        client_message_ = message;
    }
    void ResetOwnership() { client_message_ = NULL; }

    const ControllerOptions& options() const { return options_;    }
    uint32_t code()                    const { return code_;       }
    const std::string& error_text()    const { return error_text_; }

    void SetLocalAddress(std::string &addr)  { local_addr_ = addr;  }
    void SetRemoteAddress(std::string &addr) { remote_addr_ = addr; }

    void SetResponseCode(uint32_t code) { code_ = code; }
    void SetResponseError(const std::string &error) { error_text_ = error; }

private:
    pthread_t tid_;
    ControllerOptions options_;

    uint32_t code_;
    std::string error_text_;

    std::string local_addr_;
    std::string remote_addr_;

    ClientMessage *client_message_;
};

} // namespace qrpc

#endif /* QRPC_RPC_CONTROLLER_CLIENT_H */
