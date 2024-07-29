/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FastFloat.h"

#include "fast_float/fast_float.h"

namespace WTF {

double parseDouble(const LChar* string, size_t length, size_t& parsedLength)
{
    double doubleValue = 0;
    auto result = fast_float::from_chars(reinterpret_cast<const char*>(string), reinterpret_cast<const char*>(string) + length, doubleValue);
    parsedLength = result.ptr - reinterpret_cast<const char*>(string);
    return doubleValue;
}

double parseDouble(const UChar* string, size_t length, size_t& parsedLength)
{
    double doubleValue = 0;
    auto result = fast_float::from_chars(reinterpret_cast<const char16_t*>(string), reinterpret_cast<const char16_t*>(string) + length, doubleValue);
    parsedLength = result.ptr - reinterpret_cast<const char16_t*>(string);
    return doubleValue;
}

} // namespace WTF
