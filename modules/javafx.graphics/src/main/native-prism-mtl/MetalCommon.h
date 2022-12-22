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

#ifndef METAL_COMMON_H
#define METAL_COMMON_H

#import <jni.h>
#import <simd/simd.h>

#define jlong_to_ptr(value) (intptr_t)value
#define ptr_to_jlong(value) (jlong)((intptr_t)value)

#define ENABLE_VERBOSE 1
#define METAL_VERBOSE 1

#ifdef ENABLE_VERBOSE
#define TEX_VERBOSE
#define CTX_VERBOSE
#define SHADER_VERBOSE
#define METAL_VERBOSE
#endif

#ifdef METAL_VERBOSE
#define METAL_LOG NSLog
#else
#define METAL_LOG(...)
#endif

#endif
