/*
 * Copyright (c) 2011, 2022, Oracle and/or its affiliates. All rights reserved.
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

package com.sun.javafx.tk.quantum;

import java.nio.IntBuffer;
import com.sun.glass.ui.Pixels;
import com.sun.prism.Graphics;
import com.sun.prism.GraphicsPipeline;
import com.sun.prism.RTTexture;
import com.sun.prism.Texture.WrapMode;
import com.sun.prism.impl.Disposer;
import com.sun.prism.impl.QueuedPixelSource;
import com.sun.prism.mtl.MTLContext;
import com.sun.prism.mtl.MTLRTTexture;
import com.sun.prism.mtl.MTLResourceFactory;
import com.sun.prism.mtl.MTLSwapChain;

/**
 * UploadingPainter is used when we need to render into an offscreen buffer.
 * The PresentingPainter is used when we are rendering to the main screen.
 */
// TODO: MTL: Modifications in this class are a temporary workaround to enable the POC work of Metal.
// It should be reverted/corrected along with the implementation of SwapChain and CAMetalLayer.
// Eventually we expect no modifications to this class.
final class UploadingPainter extends ViewPainter {

    private RTTexture   rttexture;
    // resolveRTT is a temporary render target to "resolve" a msaa render buffer
    // into a normal color render target.
    private RTTexture   resolveRTT = null;

    private QueuedPixelSource pixelSource = new QueuedPixelSource(true);
    private float penScaleX, penScaleY;

    UploadingPainter(GlassScene view) {
        super(view);
    }

    void disposeRTTexture() {
        if (rttexture != null) {
            rttexture.dispose();
            rttexture = null;
        }
        if (resolveRTT != null) {
            resolveRTT.dispose();
            resolveRTT = null;
        }
    }

    @Override
    public float getPixelScaleFactorX() {
        return sceneState.getRenderScaleX();
    }

    @Override
    public float getPixelScaleFactorY() {
        return sceneState.getRenderScaleY();
    }

    @Override public void run() {
        renderLock.lock();

        boolean errored = false;
        try {
            if (!validateStageGraphics()) {
                if (QuantumToolkit.verbose) {
                    System.err.println("UploadingPainter: validateStageGraphics failed");
                }
                paintImpl(null);
                return;
            }

            if (factory == null) {
                factory = GraphicsPipeline.getDefaultResourceFactory();
            }
            if (factory == null || !factory.isDeviceReady()) {
                factory = null;
                return;
            }
            //System.out.println("Factory : " + factory);

            float scalex = getPixelScaleFactorX();
            float scaley = getPixelScaleFactorY();
            int bufWidth = sceneState.getRenderWidth();
            int bufHeight = sceneState.getRenderHeight();

            // Repaint everything on pen scale or view size change because
            // texture contents are no longer correct.
            // Repaint everything on new texture dimensions because otherwise
            // our upload logic below may fail with index out of bounds.
            boolean needsReset = (penScaleX != scalex ||
                                  penScaleY != scaley ||
                                  penWidth != viewWidth ||
                                  penHeight != viewHeight ||
                                  rttexture == null ||
                                  rttexture.getContentWidth() != bufWidth ||
                                  rttexture.getContentHeight() != bufHeight);

            if (!needsReset) {
                rttexture.lock();
                if (rttexture.isSurfaceLost()) {
                    rttexture.unlock();
                    sceneState.getScene().entireSceneNeedsRepaint();
                    needsReset = true;
                }
            }

            if (needsReset) {
                disposeRTTexture();
                rttexture = factory.createRTTexture(bufWidth, bufHeight,
                        WrapMode.CLAMP_NOT_NEEDED, sceneState.isMSAA());
                if (rttexture == null) {
                    return;
                }

                // create a presentable,  MTLSwapChain
                presentable = new MTLSwapChain(((MTLResourceFactory) factory).getContext(),
                        0l, (MTLRTTexture) rttexture, scalex, scaley);

                penScaleX   = scalex;
                penScaleY   = scaley;
                penWidth    = viewWidth;
                penHeight   = viewHeight;
                freshBackBuffer = true;
            }
            Graphics g = rttexture.createGraphics();
            if (g == null) {
                disposeRTTexture();
                sceneState.getScene().entireSceneNeedsRepaint();
                return;
            }
            g.scale(scalex, scaley);
            paintImpl(g);
            presentable.prepare(null);
            freshBackBuffer = false;

            int outWidth = sceneState.getOutputWidth();
            int outHeight = sceneState.getOutputHeight();
            float outScaleX = sceneState.getOutputScaleX();
            float outScaleY = sceneState.getOutputScaleY();
            RTTexture rtt = rttexture;
            /*if (rttexture.isMSAA() || outWidth != bufWidth || outHeight != bufHeight) {
                rtt = resolveRenderTarget(g, outWidth, outHeight);
            } else {
                rtt = rttexture;
            }*/

            Pixels pix = pixelSource.getUnusedPixels(outWidth, outHeight, outScaleX, outScaleY);
            IntBuffer bits = (IntBuffer) pix.getPixels();
            rtt.readPixels(bits);

            /*System.err.println("UploadingPainter() -> Reading rtt pixels " +
                    "bits.capacity(): " + bits.capacity() +
                    ", outWidth: " + outWidth + ", outHeight: " + outHeight +
                    ", outScaleX: " + outScaleX + ", outScaleY: " + outScaleY);*/
            //int rawbits[] = rtt.getPixels();
            //int dim = bits.array();
            /*int rawbits[] = new int[bits.capacity()];
            for (int i = 0; (i + 3) < rawbits.length; i += 4) {
                rawbits[i + 0] = 0xFF;
                rawbits[i + 1] = 0x00;
                rawbits[i + 2] = 0xFF;
                rawbits[i + 3] = 0x00;
            }*/
            //if (rawbits != null) {
            //    bits.put(rawbits, 0, outWidth * outHeight);
            //} else {
                /*if (!rtt.readPixels(bits)) {
                    // device lost
                    sceneState.getScene().entireSceneNeedsRepaint();
                    disposeRTTexture();
                    pix = null;
                }*/
            //}

            if (rttexture != null) {
                rttexture.unlock();
            }

            if (pix != null) {
                /* transparent pixels created and ready for upload */
                // Copy references, which are volatile, used by upload. Thus
                // ensure they still exist once event queue is consumed.
                pixelSource.enqueuePixels(pix);
                sceneState.uploadPixels(pixelSource);
            }

        } catch (Throwable th) {
            errored = true;
            th.printStackTrace(System.err);
        } finally {
            if (rttexture != null && rttexture.isLocked()) {
                rttexture.unlock();
            }
            if (resolveRTT != null && resolveRTT.isLocked()) {
                resolveRTT.unlock();
            }

            Disposer.cleanUp();

            sceneState.getScene().setPainting(false);

            if (factory != null) {
                //factory.getTextureResourcePool().freeDisposalRequestedAndCheckResources(errored);
            }

            renderLock.unlock();
        }
    }

    private RTTexture resolveRenderTarget(Graphics g, int width, int height) {
        if (resolveRTT != null) {
            resolveRTT.lock();
            if (resolveRTT.isSurfaceLost() ||
                resolveRTT.getContentWidth() != width ||
                resolveRTT.getContentHeight() != height)
            {
                resolveRTT.unlock();
                resolveRTT.dispose();
                resolveRTT = null;
            }
        }
        if (resolveRTT == null) {
            resolveRTT = g.getResourceFactory().createRTTexture(
                    width, height,
                    WrapMode.CLAMP_NOT_NEEDED, false);
        }
        int srcw = rttexture.getContentWidth();
        int srch = rttexture.getContentHeight();
        g.blit(rttexture, resolveRTT, 0, 0, srcw, srch, 0, 0, width, height);
        return resolveRTT;
    }
}
