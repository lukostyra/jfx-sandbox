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

import com.sun.prism.impl.BaseGraphicsResource;
import com.sun.prism.impl.Disposer;

/**
 * This class provides base functionality for tracking and releasing native
 * Metal pipeline resources.
 * See D3DResource for reference.
 *
 * // TODO: MTL: Complete implementation of this class.
 *
 * */

public class MTLResource extends BaseGraphicsResource {

    protected final MTLRecord mtlResRecord;

    MTLResource(MTLRecord disposerRecord) {
        super(disposerRecord);
        this.mtlResRecord = disposerRecord;
    }

    @Override
    public void dispose() {
        mtlResRecord.dispose();
    }

    static class MTLRecord implements Disposer.Record {

        private final MTLContext context;
        private long pResource;

        MTLRecord(MTLContext context, long pResource) {
            this.context = context;
            this.pResource = pResource;
            if (pResource != 0L) {
                // TODO: MTL: Implement addRecord() in MTLResourceFactory
                // context.getResourceFactory().addRecord(this);
            }
        }

        public MTLContext getContext() {
            return context;
        }

        @Override
        public void dispose() {
            if (pResource != 0L) {
                // TODO: MTL: Implement removeRecord() in MTLResourceFactory
                // context.getResourceFactory().removeRecord(this);
                MTLResourceFactory.releaseResource(context, pResource);
                pResource = 0L;
            }
        }
    }
}