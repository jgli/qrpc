#include <stdint.h>
#include <string>

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "src/qrpc/rpc/closure.h"

using namespace std;
using namespace google::protobuf;

namespace qrpc {

Closure::~Closure()
{

}

} // namespace qrpc
