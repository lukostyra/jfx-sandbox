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

#import <jni.h>
#import <dlfcn.h>
#import <stdlib.h>
#import <assert.h>
#import <stdio.h>
#import <string.h>
#import <math.h>

#import "MetalContext.h"
#import "MetalRTTexture.h"
#import "MetalResourceFactory.h"
#import "MetalPipelineManager.h"
#import "MetalShader.h"
#import "com_sun_prism_mtl_MTLContext.h"

#ifdef CTX_VERBOSE
#define CTX_LOG NSLog
#else
#define CTX_LOG(...)
#endif

@implementation MetalContext

- (id) createContext:(NSString*)shaderLibPath
{
    CTX_LOG(@"-> MetalContext.createContext()");
    self = [super init];
    if (self) {
        // RTT must be cleared before drawing into it for the first time,
        // after drawing first time the loadAction shall be set to MTLLoadActionLoad
        // so that it becomes a stable rtt.
        // loadAction is set to MTLLoadActionLoad in [resetRenderPass]
        rttLoadAction = MTLLoadActionClear;

        device = MTLCreateSystemDefaultDevice();
        commandQueue = [device newCommandQueue];
        commandQueue.label = @"The only MTLCommandQueue";
        pipelineManager = [MetalPipelineManager alloc];
        [pipelineManager init:self libPath:shaderLibPath];

        rttPassDesc = [MTLRenderPassDescriptor new];
        rttPassDesc.colorAttachments[0].loadAction = rttLoadAction;
        rttPassDesc.colorAttachments[0].clearColor = MTLClearColorMake(1, 1, 1, 1); // make this programmable
        rttPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
    }
    return self;
}

- (void) setRTT:(MetalRTTexture*)rttPtr
{
    CTX_LOG(@"-> Native: MetalContext.setRTT()");
    rtt = rttPtr;
    rttPassDesc.colorAttachments[0].texture = [rtt getTexture];
}

- (MetalRTTexture*) getRTT
{
    CTX_LOG(@"-> Native: MetalContext.getRTT()");
    return rtt;
}


- (void) setTex0:(MetalTexture*)texPtr
{
    CTX_LOG(@"-> Native: MetalContext.setTex0() ----- %p", texPtr);
    tex0 = texPtr;
}

- (MetalTexture*) getTex0
{
    CTX_LOG(@"-> Native: MetalContext.getTex0()");
    return tex0;
}


- (id<MTLDevice>) getDevice
{
    // CTX_LOG(@"MetalContext.getDevice()");
    return device;
}

- (id<MTLCommandBuffer>) newCommandBuffer
{
    CTX_LOG(@"MetalContext.newCommandBuffer()");
    currentCommandBuffer = [self newCommandBuffer:@"Command Buffer"];
    return currentCommandBuffer;
}

- (id<MTLCommandBuffer>) newCommandBuffer:(NSString*)label
{
    CTX_LOG(@"MetalContext.newCommandBufferWithLabel()");
    currentCommandBuffer = [commandQueue commandBuffer];
    currentCommandBuffer.label = label;
    [currentCommandBuffer addScheduledHandler:^(id<MTLCommandBuffer> cb) {
         CTX_LOG(@"------------------> Native: commandBuffer Scheduled");
    }];
    [currentCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> cb) {
         currentCommandBuffer = nil;
         CTX_LOG(@"------------------> Native: commandBuffer Completed");
    }];
    return currentCommandBuffer;
}

- (id<MTLCommandBuffer>) getCurrentCommandBuffer
{
    CTX_LOG(@"MetalContext.getCurrentCommandBuffer()");
    if (currentCommandBuffer == nil) {
        return [self newCommandBuffer];
    }
    return currentCommandBuffer;
}

- (MetalResourceFactory*) getResourceFactory
{
    CTX_LOG(@"MetalContext.getResourceFactory()");
    return resourceFactory;
}

- (NSInteger) drawIndexedQuads : (struct PrismSourceVertex const *)pSrcFloats
                      ofColors : (char const *)pSrcColors
                   vertexCount : (NSUInteger)numVerts
{
    CTX_LOG(@"MetalContext.drawIndexedQuads()");

    [self fillVB:pSrcFloats colors:pSrcColors numVertices:numVerts];

    id<MTLCommandBuffer> commandBuffer = [self getCurrentCommandBuffer];
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:rttPassDesc];

    if (currentPipeState != nil) {
        CTX_LOG(@"MetalContext.drawIndexedQuads() currentShader");
        [renderEncoder setRenderPipelineState:currentPipeState];
    } else {
        CTX_LOG(@"MetalContext.drawIndexedQuads() currentShader is nil");
        id<MTLRenderPipelineState> pipeline = [pipelineManager getPipeStateWithFragFuncName:@"Solid_Color"];
        [renderEncoder setRenderPipelineState:pipeline];
    }

    [renderEncoder setVertexBytes:vertices
                           length:sizeof(vertices)
                          atIndex:VertexInputIndexVertices];

    [renderEncoder setVertexBytes:&mvpMatrix
                           length:sizeof(mvpMatrix)
                          atIndex:VertexInputMatrixMVP];

    [renderEncoder setFragmentBuffer:currentFragArgBuffer
                              offset:0
                             atIndex:0];

    if (tex0 != nil) {
        id<MTLTexture> tex = [tex0 getTexture];

        CTX_LOG(@"drawIndexedQuads tex0 = %p tex = %p", tex0, tex);

        [currentShader setTexture: @"inputTex" texture:tex];

        [renderEncoder useResource:tex usage:MTLResourceUsageRead];
    }

    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                      vertexStart:0
                      vertexCount:numTriangles * 3];

    [renderEncoder endEncoding];

    [commandBuffer commit];

    [self resetRenderPass];

    [commandBuffer waitUntilCompleted];

    return 1;
}

- (void) setProjViewMatrix:(bool)isOrtho
        m00:(float)m00 m01:(float)m01 m02:(float)m02 m03:(float)m03
        m10:(float)m10 m11:(float)m11 m12:(float)m12 m13:(float)m13
        m20:(float)m20 m21:(float)m21 m22:(float)m22 m23:(float)m23
        m30:(float)m30 m31:(float)m31 m32:(float)m32 m33:(float)m33
{
    CTX_LOG(@"MetalContext.setProjViewMatrix()");
    mvpMatrix = simd_matrix(
        (simd_float4){ m00, m01, m02, m03 },
        (simd_float4){ m10, m11, m12, m13 },
        (simd_float4){ m20, m21, m22, m23 },
        (simd_float4){ m30, m31, m32, m33 }
    );
}

- (void) resetRenderPass
{
    CTX_LOG(@"MetalContext.resetRenderPass()");
    rttPassDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
}

- (void) setRTTLoadActionToClear
{
    CTX_LOG(@"MetalContext.clearRTT()");
    rttPassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
}

- (void) resetProjViewMatrix
{
    CTX_LOG(@"MetalContext.resetProjViewMatrix()");
    mvpMatrix = simd_matrix(
        (simd_float4){1, 0, 0, 0},
        (simd_float4){0, 1, 0, 0},
        (simd_float4){0, 0, 1, 0},
        (simd_float4){0, 0, 0, 1}
    );
}

- (void) fillVB:(struct PrismSourceVertex const *)pSrcFloats colors:(char const *)pSrcColors
                 numVertices:(int)numVerts
{
    VS_INPUT* pVert = vertices;
    numTriangles = numVerts / 2;
    int numQuads = numTriangles / 2;

    for (int i = 0; i < numQuads; i++) {
        unsigned char const* colors = (unsigned char*)(pSrcColors + i * 4 * 4);
        struct PrismSourceVertex const * inVerts = pSrcFloats + i * 4;
        for (int k = 0; k < 2; k++) {
            for (int j = 0; j < 3; j++) {
                pVert->position.x = inVerts->x;
                pVert->position.y = inVerts->y;

                pVert->color.x = ((float)(*(colors)))/255.0f;
                pVert->color.y = ((float)(*(colors + 1)))/255.0f;
                pVert->color.z = ((float)(*(colors + 2)))/255.0f;
                pVert->color.w = 1.0f;

                pVert->texCoord0.x = inVerts->tu1;
                pVert->texCoord0.y = inVerts->tv1;

                pVert->texCoord1.x = inVerts->tu2;
                pVert->texCoord1.y = inVerts->tv2;

                inVerts++;
                colors += 4;
                pVert++;
            }
            inVerts -= 2;
            colors -= 4; colors -= 4;
        }
    }
}

- (MetalPipelineManager*) getPipelineManager
{
    return pipelineManager;
}

- (void) setCurrentPipeState:(id<MTLRenderPipelineState>) pipeState
{
    currentPipeState = pipeState;
}

- (void) setCurrentArgumentBuffer:(id<MTLBuffer>) argBuffer
{
    currentFragArgBuffer = argBuffer;
}

- (MetalShader*) getCurrentShader
{
    return currentShader;
}

- (void) setCurrentShader:(MetalShader*) shader
{
    currentShader = shader;
}

// TODO: MTL: This was copied from GlassHelper, and could be moved to a utility class.
+ (NSString*) nsStringWithJavaString:(jstring)javaString withEnv:(JNIEnv*)env
{
    NSString *string = @"";
    if (javaString != NULL) {
        const jchar* jstrChars = (*env)->GetStringChars(env, javaString, NULL);
        jsize size = (*env)->GetStringLength(env, javaString);
        if (size > 0) {
            string = [[[NSString alloc] initWithCharacters:jstrChars length:(NSUInteger)size] autorelease];
        }
        (*env)->ReleaseStringChars(env, javaString, jstrChars);
    }
    return string;
}

@end // MetalContext


JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLContext_nInitialize
  (JNIEnv *env, jclass jClass, jstring shaderLibPathStr)
{
    CTX_LOG(@">>>> MTLContext_nInitialize");
    jlong jContextPtr = 0L;
    NSString* shaderLibPath = [MetalContext nsStringWithJavaString:shaderLibPathStr withEnv:env];
    CTX_LOG(@"----> shaderLibPath: %@", shaderLibPath);
    jContextPtr = ptr_to_jlong([[MetalContext alloc] createContext:shaderLibPath]);
    CTX_LOG(@"<<<< MTLContext_nInitialize");
    return jContextPtr;
}

JNIEXPORT jint JNICALL Java_com_sun_prism_mtl_MTLContext_nDrawIndexedQuads
  (JNIEnv *env, jclass jClass, jlong context, jfloatArray vertices, jbyteArray colors, jint numVertices)
{
    CTX_LOG(@"MTLContext_nDrawIndexedQuads");

    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);

    struct PrismSourceVertex *pVertices =
                    (struct PrismSourceVertex *) (*env)->GetPrimitiveArrayCritical(env, vertices, 0);
    char *pColors = (char *) (*env)->GetPrimitiveArrayCritical(env, colors, 0);

    [mtlContext drawIndexedQuads:pVertices ofColors:pColors vertexCount:numVertices];

    if (pColors) (*env)->ReleasePrimitiveArrayCritical(env, colors, pColors, 0);
    if (pVertices) (*env)->ReleasePrimitiveArrayCritical(env, vertices, pVertices, 0);

    return 1;
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nUpdateRenderTarget
  (JNIEnv *env, jclass jClass, jlong context, jlong texPtr)
{
    CTX_LOG(@"MTLContext_nUpdateRenderTarget");
    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);
    MetalRTTexture *rtt = (MetalRTTexture *)jlong_to_ptr(texPtr);
    [mtlContext setRTT:rtt];
}

JNIEXPORT jint JNICALL Java_com_sun_prism_mtl_MTLContext_nResetTransform
  (JNIEnv *env, jclass jClass, jlong context)
{
    CTX_LOG(@"MTLContext_nResetTransform");

    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);
    //[mtlContext resetProjViewMatrix];
    return 1;
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetTex0
  (JNIEnv *env, jclass jClass, jlong context, jlong texPtr)
{
    CTX_LOG(@"MTLContext_nSetTex0 : texPtr = %ld", texPtr);
    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);
    MetalTexture *tex = (MetalTexture *)jlong_to_ptr(texPtr);
    [mtlContext setTex0:tex];
}

JNIEXPORT jint JNICALL Java_com_sun_prism_mtl_MTLContext_nSetProjViewMatrix
  (JNIEnv *env, jclass jClass,
    jlong context, jboolean isOrtho,
    jdouble m00, jdouble m01, jdouble m02, jdouble m03,
    jdouble m10, jdouble m11, jdouble m12, jdouble m13,
    jdouble m20, jdouble m21, jdouble m22, jdouble m23,
    jdouble m30, jdouble m31, jdouble m32, jdouble m33)
{
    CTX_LOG(@"MTLContext_nSetProjViewMatrix");
    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);

    CTX_LOG(@"%f %f %f %f", m00, m01, m02, m03);
    CTX_LOG(@"%f %f %f %f", m10, m11, m12, m13);
    CTX_LOG(@"%f %f %f %f", m20, m21, m22, m23);
    CTX_LOG(@"%f %f %f %f", m30, m31, m32, m33);

    [mtlContext setProjViewMatrix:isOrtho
        m00:m00 m01:m01 m02:m02 m03:m03
        m10:m10 m11:m11 m12:m12 m13:m13
        m20:m20 m21:m21 m22:m22 m23:m23
        m30:m30 m31:m31 m32:m32 m33:m33];

    return 1;
}
