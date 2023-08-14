/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "CSSUnresolvedColorMix.h"

#include "ColorFromPrimitiveValue.h"
#include "ColorSerialization.h"
#include "StyleBuilderState.h"

namespace WebCore {

static void serializationForCSS(StringBuilder& builder, const CSSUnresolvedColorMix::Component& component)
{
    builder.append(component.color->customCSSText());
    if (component.percentage)
        builder.append(' ', component.percentage->customCSSText());
}

void serializationForCSS(StringBuilder& builder, const CSSUnresolvedColorMix& unresolved)
{
    builder.append("color-mix(in ");
    serializationForCSS(builder, unresolved.colorInterpolationMethod);
    builder.append(", ");
    serializationForCSS(builder, unresolved.mixComponents1);
    builder.append(", ");
    serializationForCSS(builder, unresolved.mixComponents2);
    builder.append(')');
}

String serializationForCSS(const CSSUnresolvedColorMix& unresolved)
{
    StringBuilder builder;
    serializationForCSS(builder, unresolved);
    return builder.toString();
}

static bool operator==(const CSSUnresolvedColorMix::Component& a, const CSSUnresolvedColorMix::Component& b)
{
    return compareCSSValue(a.color, b.color)
        && compareCSSValuePtr(a.percentage, b.percentage);
}

bool operator==(const CSSUnresolvedColorMix& a, const CSSUnresolvedColorMix& b)
{
    return a.colorInterpolationMethod == b.colorInterpolationMethod
        && a.mixComponents1 == b.mixComponents1
        && a.mixComponents2 == b.mixComponents2;
}

bool operator!=(const CSSUnresolvedColorMix& a, const CSSUnresolvedColorMix& b)
{
    return !(a == b);
}

StyleColor createStyleColor(const CSSUnresolvedColorMix& unresolved, const Document& document, RenderStyle& style, Style::ForVisitedLink forVisitedLink)
{
    auto resolvePercentage = [](auto& percentage) -> std::optional<double> {
        if (!percentage)
            return std::nullopt;
        return percentage->doubleValue();
    };

    auto resolveComponent = [&](auto& component, auto& document, auto& style, auto forVisitedLink) -> StyleColorMix::Component {
        return {
            colorFromPrimitiveValue(document, style, component.color.get(), forVisitedLink),
            resolvePercentage(component.percentage)
        };
    };

    return StyleColor {
        StyleColorMix {
            unresolved.colorInterpolationMethod,
            resolveComponent(unresolved.mixComponents1, document, style, forVisitedLink),
            resolveComponent(unresolved.mixComponents2, document, style, forVisitedLink)
        }
    };
}

} // namespace WebCore
