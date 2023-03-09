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

import com.sun.glass.ui.Screen;
import com.sun.prism.*;
import com.sun.prism.impl.PrismSettings;
import com.sun.prism.impl.TextureResourcePool;
import com.sun.prism.impl.ps.BaseShaderFactory;
import com.sun.prism.ps.Shader;
import com.sun.prism.ps.ShaderFactory;

import java.io.InputStream;
import java.io.ByteArrayInputStream;
import java.util.Map;
import java.lang.reflect.Method;

public class MTLResourceFactory extends BaseShaderFactory {
    private final MTLContext context;

    MTLResourceFactory(Screen screen) {
        MTLLog.Debug("MTLResourceFactory(): screen = " + screen);
        MTLLog.Debug(">>> MTLResourceFactory()");
        context = new MTLContext(screen, this);
        // TODO: MTL: Move mtl library creation from MetalContext Ctor such that it happens only once
        // can use a static method in Java class/ or a flag

        if (PrismSettings.noClampToZero && PrismSettings.verbose) {
            MTLLog.Debug("prism.noclamptozero not supported by MTL");
        }
        MTLLog.Debug("<<< MTLResourceFactory()");
    }

    public MTLContext getContext() {
        return context;
    }

    @Override
    public Shader createShader(InputStream shaderNameStream, Map<String, Integer> samplers,
                               Map<String, Integer> params, int maxTexCoordIndex,
                               boolean isPixcoordUsed, boolean isPerVertexColorUsed) {
        try {
            String shaderName = new String(shaderNameStream.readAllBytes());
            return createShader(shaderName, samplers, params, maxTexCoordIndex,
                                isPixcoordUsed, isPerVertexColorUsed);
        } catch (Exception e) {
            throw new UnsupportedOperationException("Failed to create a prism shader");
        }
    }

    @Override
    public Shader createShader(String shaderName, Map<String, Integer> samplers,
                               Map<String, Integer> params, int maxTexCoordIndex,
                               boolean isPixcoordUsed, boolean isPerVertexColorUsed) {
        MTLLog.Debug(">>> MTLResourceFactory.createShader()");
        MTLLog.Debug("    shaderName: " + shaderName);
        MTLLog.Debug("    samplers: " + samplers);
        MTLLog.Debug("    params: " + params);
        MTLLog.Debug("    maxTexCoordIndex: " + maxTexCoordIndex);
        MTLLog.Debug("    isPixcoordUsed: " + isPixcoordUsed);
        MTLLog.Debug("    isPerVertexColorUsed: " + isPerVertexColorUsed);
        Shader shader = MTLShader.createShader(getContext(), shaderName, samplers,
                params, maxTexCoordIndex, isPixcoordUsed, isPerVertexColorUsed);
        MTLLog.Debug("<<< MTLResourceFactory.createShader()");
        return shader;
    }

    @Override
    public Shader createStockShader(String shaderName) {
        if (shaderName == null) {
            throw new IllegalArgumentException("Shader name must be non-null");
        }
        try {
            if (PrismSettings.verbose) {
                System.out.println("MTLResourceFactory: Prism - createStockShader: " + shaderName);
            }
            Class klass = Class.forName("com.sun.prism.shader." + shaderName + "_Loader");
            Method m = klass.getMethod("loadShader", new Class[] {ShaderFactory.class, InputStream.class});
            InputStream nameStream = new ByteArrayInputStream(shaderName.getBytes());
            return (Shader) m.invoke(null, new Object[]{this, nameStream});
        } catch (Throwable e) {
            e.printStackTrace();
            throw new InternalError("Error loading stock shader " + shaderName);
        }
    }

    @Override
    public TextureResourcePool getTextureResourcePool() {
        return MTLVramPool.getInstance();
    }

    @Override
    public Texture createTexture(PixelFormat formatHint, Texture.Usage usageHint,
                                 Texture.WrapMode wrapMode, int w, int h) {
        // TODO: MTL: Complete implementation
        return createTexture(formatHint, usageHint, wrapMode, w,h, false);
    }

    @Override
    public Texture createTexture(PixelFormat formatHint, Texture.Usage usageHint,
                                 Texture.WrapMode wrapMode, int w, int h, boolean useMipmap) {

        int createw = w;
        int createh = h;

        // createw = nextPowerOf64(createw, 8192);
        // createh = nextPowerOf64(createh, 8192);

        long pResource = nCreateTexture(context.getContextHandle() ,
                (int) formatHint.ordinal(), (int) usageHint.ordinal(),
                true, createw, createh, 1, false);

        if (pResource == 0L) {
            return null;
        }

        MTLTextureData textData = new MTLTextureData(context, pResource);
        MTLTextureResource resource = new MTLTextureResource(textData);

        // TODO: MTL: contentX and contentY is set as 0 - please see ES2/D3D path and try to match it
        return new MTLTexture(getContext(), resource, formatHint, wrapMode, createw, createh, 0, 0, createw, createh, useMipmap);
    }

    @Override
    public Texture createTexture(MediaFrame frame) {
        return null;
    }

    @Override
    public boolean isFormatSupported(PixelFormat format) {
        switch (format) {
            case BYTE_RGB:
            case BYTE_GRAY:
            case BYTE_ALPHA:
            case BYTE_BGRA_PRE:
            case INT_ARGB_PRE:
            case FLOAT_XYZW:
                return true;
            case MULTI_YCbCr_420:
            case BYTE_APPLE_422:
            default:
                return false;
        }
    }

    @Override
    public int getMaximumTextureSize() {
        // TODO: MTL: Complete implementation
        // This value should be fetched from the MTLDevice.

        // This value comes from Metal feature set tables
        return 16384; // For MTLGPUFamilyApple3 and above
    }

    @Override
    public int getRTTWidth(int w, Texture.WrapMode wrapMode) {
        int rttWidth = nextPowerOf64(w, 8192);
        return rttWidth;
    }

    @Override
    public int getRTTHeight(int h, Texture.WrapMode wrapMode) {
        int rttHeight = nextPowerOf64(h, 8192);
        return rttHeight;
    }

    @Override
    public RTTexture createRTTexture(int width, int height, Texture.WrapMode wrapMode) {
        return createRTTexture(width, height, wrapMode, false);
    }

    static int nextPowerOf64(int val, int max) {
        // Using a random value for width or height of texture results in this error:
        // -> validateStrideTextureParameters:1512: failed assertion
        // -> Linear texture: bytesPerRow (XXXX) must be aligned to 256 bytes,
        // This implies that the width and height of a texture must be multiple of 64 pixels.
        if (val > max) {
            return 0;
        }
        int minPixelsRow = 64;
        if (val % minPixelsRow != 0) {
            int times = val / minPixelsRow;
            val = minPixelsRow * (times + 1);
        }
        return val;
    }

    public RTTexture createRTTexture(int width, int height, Texture.WrapMode wrapMode, boolean msaa) {
        MTLLog.Debug("MTLResourceFactory.createRTTexture(): width = " + width +
                ", height = " + height + ", wrapMode = " + wrapMode + ", msaa = " + msaa);
        int createw = width;
        int createh = height;
        int cx = 0;
        int cy = 0;

        createw = nextPowerOf64(createw, 8192);
        createh = nextPowerOf64(createh, 8192);

        MTLRTTexture rtt = MTLRTTexture.create(context, createw, createh, width, height, wrapMode, msaa);
        return rtt;
    }

    @Override
    public boolean isCompatibleTexture(Texture tex) {
        // TODO: MTL: Complete implementation
        return false;
    }

    @Override
    public Presentable createPresentable(PresentableState pState) {
        // TODO: MTL: Complete implementation
        return null;
    }

    @Override
    public void dispose() {
        // This is simply invoking super method as of now.
        // TODO: MTL: Complete implementation
        MTLLog.Debug("MTLResourceFactory dispose is invoked");
        super.dispose();
    }

    @Override
    public PhongMaterial createPhongMaterial() {
        if (checkDisposed()) return null;
        return MTLPhongMaterial.create(context);
    }

    @Override
    public MeshView createMeshView(Mesh mesh) {
        if (checkDisposed()) return null;
        return MTLMeshView.create(context, (MTLMesh) mesh);
    }

    @Override
    public Mesh createMesh() {
        if (checkDisposed()) return null;
        return MTLMesh.create(context);
    }

    static native long nCreateTexture(long pContext,
                                      int format, int hint,
                                      boolean isRTT,
                                      int width, int height, int samples,
                                      boolean useMipmap);

    static native void nReleaseTexture(long context, long pTexture);

    static void releaseResource(MTLContext context, long resource) {
    }

    static void releaseTexture(MTLContext context, long resource) {
        nReleaseTexture(context.getContextHandle(), resource);
    }
}
