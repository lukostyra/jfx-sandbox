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

#pragma once

#include "AnimationEventBase.h"
#include "RenderStyleConstants.h"

namespace WebCore {

class StyleOriginatedAnimationEvent : public AnimationEventBase {
    WTF_MAKE_ISO_ALLOCATED(StyleOriginatedAnimationEvent);
public:
    virtual ~StyleOriginatedAnimationEvent();

    double elapsedTime() const { return m_elapsedTime; }
    const String& pseudoElement();
    PseudoId pseudoId() const { return m_pseudoId; }

protected:
    StyleOriginatedAnimationEvent(const AtomString& type, WebAnimation*, std::optional<Seconds> scheduledTime, double, PseudoId);
    StyleOriginatedAnimationEvent(const AtomString&, const EventInit&, IsTrusted, double, const String&);

private:
    double m_elapsedTime;
    String m_pseudoElement;
    PseudoId m_pseudoId { PseudoId::None };
};

} // namespace WebCore
