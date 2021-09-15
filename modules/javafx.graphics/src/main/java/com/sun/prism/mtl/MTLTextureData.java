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

import com.sun.prism.impl.Disposer;

import java.util.Objects;

public class MTLTextureData implements Disposer.Record {
    private final MTLContext mtlContext;
    private long pTexture;
    private int size;

    // MTLBuffer used to store the pixel data of this texture
    // private long nTexPixelData;

    private MTLTextureData() {
        mtlContext = null;
    }

    MTLTextureData(MTLContext context, long texPtr) {
        Objects.requireNonNull(context);
        if (texPtr <= 0) {
            throw new IllegalArgumentException("Texture cannot be null");
        }
        mtlContext = context;
        pTexture = texPtr;
    }

    public void setResource(long resource) {
        pTexture = resource;
    }

    public long getResource() {
        return pTexture;
    }

    public long getSize() {
        return size;
    }

    @Override
    public void dispose() {
        if (pTexture != 0) {
            MTLResourceFactory.releaseTexture(mtlContext, pTexture);
            pTexture = 0;
        }
    }
}
