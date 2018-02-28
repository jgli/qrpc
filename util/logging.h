// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Must not be included from any .h files to avoid polluting the namespace
// with macros.

#ifndef QRPC_UTIL_LOGGING_H
#define QRPC_UTIL_LOGGING_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string>

namespace qrpc {

class Slice;

/* Append a human-readable printout of "num" to *str */
extern void AppendNumberTo(std::string *str, uint64_t num, bool align);

/*
 * Append a human-readable printout of "value" to *str.
 * Escapes any non-printable characters found in "value".
 */
extern void AppendEscapedStringTo(std::string *str, const Slice &value);

/* Return a human-readable printout of "num" */
extern std::string NumberToString(uint64_t num);

/*
 * Return a human-readable version of "value".
 * Escapes any non-printable characters found in "value".
 */
extern std::string EscapeString(const Slice &value);

/*
 * Parse a human-readable number from "*in" into *value.  On success,
 * advances "*in" past the consumed number and sets "*val" to the
 * numeric value.  Otherwise, returns false and leaves *in in an
 * unspecified state.
 */
extern bool ConsumeDecimalNumber(Slice* in, uint64_t* val);

} // namespace qrpc

#endif /* QRPC_UTIL_LOGGING_H */
