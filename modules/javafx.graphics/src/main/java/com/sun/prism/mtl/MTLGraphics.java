/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package com.sun.prism.mtl;

import com.sun.prism.RenderTarget;
import com.sun.prism.impl.ps.BaseShaderGraphics;
import com.sun.prism.paint.Color;

public class MTLGraphics extends BaseShaderGraphics {

    private final MTLContext context;

    private MTLGraphics(MTLContext context, RenderTarget target) {
        super(context, target);
        this.context = context;
        MTLLog.Debug("MTLGraphics(): context = " + context + ", target = " + target);
    }

    static MTLGraphics create(MTLContext context, RenderTarget target) {
        if (target == null) {
            return null;
        }
        MTLLog.Debug("MTLGraphics.create(): context = " + context + ", target = " + target);
        return new MTLGraphics(context, target);
    }

    @Override
    public void clear(Color color) {
        MTLLog.Debug("MTLGraphics.clear(): color = " + color);

        // TODO: MTL: Remove this if condition once nClear() method starts clearing entire rtt texture
        if (color == Color.TRANSPARENT) {
            MTLLog.Debug("------------ clearning entire rtt to transparent ----------");

            long nativeRTTexture = ((MTLRTTexture)getRenderTarget()).getNativeHandle();
            nClearRTTexture(nativeRTTexture);
            return;
        }

        float r = color.getRedPremult();
        float g = color.getGreenPremult();
        float b = color.getBluePremult();
        float a = color.getAlpha();
        MTLLog.Debug("MTLGraphics.clear(): r = " + r + ", g = " + g + ", b = " + b + ", a = " + a);

        context.validateClearOp(this);
        getRenderTarget().setOpaque(color.isOpaque());

        int res = nClear(context.getContextHandle(),
                         color.getIntArgbPre(),  r, g, b, a, false, false);
        // TODO: MTL: verify the returned res value
    }

    @Override
    public void sync() {
        throw new UnsupportedOperationException("Not supported yet."); //To change body of generated methods, choose Tools | Templates.
    }

    private static native int nClear(long pContext, int color, float red, float green, float blue, float alpha, boolean clearDepth, boolean ignoreScissor);
    private static native void nClearRTTexture(long pNativeRTT);

}
