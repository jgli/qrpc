// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <string>

#include "src/qrpc/util/slice.h"
#include "src/qrpc/util/logging.h"

using namespace std;

namespace qrpc {

void AppendNumberTo(std::string *str, uint64_t num, bool align)
{
    assert(str != NULL);

    char buf[30];
    if (align) {
        snprintf(buf, sizeof(buf), "%020llu", (unsigned long long) num);
    } else {
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long) num);
    }

    str->append(buf);
}

void AppendEscapedStringTo(std::string *str, const Slice &value)
{
    assert(str != NULL);
    
    for (size_t i = 0; i < value.size(); i++) {
        char c = value[i];

        if (c >= ' ' && c <= '~') {
            str->push_back(c);
        } else {
            char buf[10];
            snprintf(buf, sizeof(buf), "\\x%02x",
                     static_cast<unsigned int>(c) & 0xff);
            str->append(buf);
        }
    }
}

std::string NumberToString(uint64_t num)
{
    std::string r;
    AppendNumberTo(&r, num, false);
    return r;
}

std::string EscapeString(const Slice &value)
{
    std::string r;
    AppendEscapedStringTo(&r, value);
    return r;
}

bool ConsumeDecimalNumber(Slice *in, uint64_t *val)
{
    uint64_t v = 0;
    int digits = 0;
    
    while (!in->empty()) {
        char c = (*in)[0];
        
        if (c >= '0' && c <= '9') {
            ++digits;
            const uint64_t delta = (c - '0');
            static const uint64_t kMaxUint64 = ~static_cast<uint64_t>(0);
            
            if (v > kMaxUint64 / 10 ||
                (v == kMaxUint64 / 10 && delta > kMaxUint64 % 10)) {
                // Overflow
                return false;
            }
            
            v = (v * 10) + delta;
            in->remove_prefix(1);
        } else {
            break;
        }
    }
    
    *val = v;
    return (digits > 0);
}

} // namespace qrpc
