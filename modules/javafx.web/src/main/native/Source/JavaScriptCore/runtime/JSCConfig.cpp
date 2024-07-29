/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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
#include "JSCConfig.h"
#include <wtf/WTFConfig.h>

namespace JSC {

#if !ENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)
Config g_jscConfig;
#endif

#if OS(WINDOWS) && ENABLE(WEBASSEMBLY)
WTF::Config g_wtfConfigForLLInt = g_wtfConfig;
#endif

Config& Config::singleton()
{
    return g_jscConfig;
}

void Config::disableFreezingForTesting()
{
    RELEASE_ASSERT(!g_jscConfig.isPermanentlyFrozen());
    g_jscConfig.disabledFreezingForTesting = true;
}

void Config::enableRestrictedOptions()
{
    RELEASE_ASSERT(!g_jscConfig.isPermanentlyFrozen());
    g_jscConfig.restrictedOptionsEnabled = true;
}

} // namespace JSC
