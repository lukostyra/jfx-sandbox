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
#import "MetalMesh.h"
#import "MetalPhongShader.h"
#import "MetalMeshView.h"
#import "MetalPhongMaterial.h"

#ifdef CTX_VERBOSE
#define CTX_LOG NSLog
#else
#define CTX_LOG(...)
#endif

#define MAX_QUADS_IN_A_BATCH 14
@implementation MetalContext

- (id) createContext:(NSString*)shaderLibPath
{
    CTX_LOG(@"-> MetalContext.createContext()");
    self = [super init];
    if (self) {
        isScissorRectSet = false;
        rttClearColor = 0XFFFFFFFF;

        device = MTLCreateSystemDefaultDevice();
        commandQueue = [device newCommandQueue];
        commandQueue.label = @"The only MTLCommandQueue";
        pipelineManager = [MetalPipelineManager alloc];
        [pipelineManager init:self libPath:shaderLibPath];

        rttPassDesc = [MTLRenderPassDescriptor new];
        rttPassDesc.colorAttachments[0].clearColor  = MTLClearColorMake(1, 1, 1, 1); // make this programmable
        rttPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;

        MTLSamplerDescriptor *samplerDescriptor = [[MTLSamplerDescriptor new] autorelease];
        sampler = [device newSamplerStateWithDescriptor:samplerDescriptor];
    }
    return self;
}

- (void) setSampler:(bool)isLinear wrapMode:(int)wrapMode;
{
    CTX_LOG(@"MetalContext.setSampler()");
    MTLSamplerDescriptor *samplerDescriptor = [[MTLSamplerDescriptor new] autorelease];
    if (isLinear) {
        samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
        samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
        samplerDescriptor.mipFilter = MTLSamplerMinMagFilterLinear;
    }
    if (wrapMode != -1) {
        samplerDescriptor.rAddressMode = wrapMode;
        samplerDescriptor.sAddressMode = wrapMode;
        samplerDescriptor.tAddressMode = wrapMode;
    }

    sampler = [device newSamplerStateWithDescriptor:samplerDescriptor];
}

- (void) setRTT:(MetalRTTexture*)rttPtr
{
    rtt = rttPtr;
    CTX_LOG(@"-> Native: MetalContext.setRTT() %lu , %lu",
                    [rtt getTexture].width, [rtt getTexture].height);

    // TODO: MTL: This is temporary to support replaceRegion call in clearRTT()
    // It will be removed when we remove replaceRegion call.
    if (clearBuffer == nil) {
        int length = [rtt getTexture].width * [rtt getTexture].height;
        clearBuffer = [device newBufferWithLength:(length * 4)
                                          options:MTLResourceStorageModeShared];
        int* pixels = [clearBuffer contents];
        for (int i = 0; i < length; i++) {
            *pixels++ = rttClearColor;
        }
    }
    rttPassDesc.colorAttachments[0].texture = [rtt getTexture];
}

- (MetalRTTexture*) getRTT
{
    CTX_LOG(@"-> Native: MetalContext.getRTT()");
    return rtt;
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
    CTX_LOG(@"MetalContext.getCurrentCommandBuffer() --- current value = %p", currentCommandBuffer);
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

    CTX_LOG(@"numVerts = %lu", numVerts);

    id<MTLCommandBuffer> commandBuffer = [self getCurrentCommandBuffer];
    if (!rttCleared) {
        // RTT must be cleared before drawing into it for the first time,
        // after drawing first time the loadAction shall be set to MTLLoadActionLoad
        // so that it becomes a stable rtt.
        // loadAction is set to MTLLoadActionLoad in [resetRenderPass]
        rttPassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
    }

    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:rttPassDesc];

    if (currentPipeState != nil) {
        [renderEncoder setRenderPipelineState:currentPipeState];
    } else {
        id<MTLRenderPipelineState> pipeline = [pipelineManager getPipeStateWithFragFuncName:@"Solid_Color"];
        [renderEncoder setRenderPipelineState:pipeline];
    }

    [renderEncoder setVertexBytes:&mvpMatrix
                               length:sizeof(mvpMatrix)
                              atIndex:VertexInputMatrixMVP];

    [renderEncoder setFragmentBuffer:currentFragArgBuffer
                              offset:0
                             atIndex:0];

    if (sampler == nil) {
        MTLSamplerDescriptor *samplerDescriptor = [[MTLSamplerDescriptor new] autorelease];
        sampler = [device newSamplerStateWithDescriptor:samplerDescriptor];
    }
    [renderEncoder setFragmentSamplerState:sampler atIndex:0];
    sampler = nil;

    if (isScissorRectSet) {
        [renderEncoder setScissorRect:scissorRect];
    }

    NSMutableDictionary* textures = [[self getCurrentShader] getTexutresDict];
    for (NSString *key in textures) {
        id<MTLTexture> tex = textures[key];
        CTX_LOG(@"    Value: %@ for key: %@", tex, key);
        [renderEncoder useResource:tex usage:MTLResourceUsageRead];
    }

    int numQuads = numVerts/4;

    // size of VS_INPUT is 48 bytes
    // We can use setVertexBytes() method to pass a vertext buffer of max size 4KB
    // 4096 / 48 = 85 vertices at a time.

    // No of quads when represneted as 2 triangles of 3 vertices each = 85/6 = 14
    // We can issue 14 quads draw from a single vertex buffer batch
    // 14 quads ==> 14 * 4 vertices = 56 vertices

    // FillVB methods fills 84 vertices in vertex batch from 56 given vertices
    // Send 56 vertices at max in each iteration

    for (int i = 0; i < numQuads; i += MAX_QUADS_IN_A_BATCH) {

        int quads = MAX_QUADS_IN_A_BATCH;
        if ((i + MAX_QUADS_IN_A_BATCH) > numQuads) {
            quads = numQuads - i;
        }
        CTX_LOG(@"Quads in this iteration =========== %d", quads);

        [self fillVB:pSrcFloats + (i * 4)
              colors:pSrcColors + (i * 4 * 4)
              numVertices:quads * 4];

        [renderEncoder setVertexBytes:vertices
                               length:sizeof(vertices)
                              atIndex:VertexInputIndexVertices];

        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                          vertexStart:0
                          vertexCount:quads * 2 * 3];
    }

    [renderEncoder endEncoding];

    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
    [self resetRenderPass];

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

- (void) setWorldTransformMatrix:(float)m00
        m01:(float)m01 m02:(float)m02 m03:(float)m03
        m10:(float)m10 m11:(float)m11 m12:(float)m12 m13:(float)m13
        m20:(float)m20 m21:(float)m21 m22:(float)m22 m23:(float)m23
        m30:(float)m30 m31:(float)m31 m32:(float)m32 m33:(float)m33
{
    CTX_LOG(@"MetalContext.setWorldTransformMatrix()");
    worldMatrix = simd_matrix(
        (simd_float4){ m00, m01, m02, m03 },
        (simd_float4){ m10, m11, m12, m13 },
        (simd_float4){ m20, m21, m22, m23 },
        (simd_float4){ m30, m31, m32, m33 }
    );
}

- (void) setWorldTransformIdentityMatrix:(float)m00
        m01:(float)m01 m02:(float)m02 m03:(float)m03
        m10:(float)m10 m11:(float)m11 m12:(float)m12 m13:(float)m13
        m20:(float)m20 m21:(float)m21 m22:(float)m22 m23:(float)m23
        m30:(float)m30 m31:(float)m31 m32:(float)m32 m33:(float)m33
{
    CTX_LOG(@"MetalContext.setWorldTransformIdentityMatrix()");
    worldMatrix = simd_matrix(
        (simd_float4){ m00, m01, m02, m03 },
        (simd_float4){ m10, m11, m12, m13 },
        (simd_float4){ m20, m21, m22, m23 },
        (simd_float4){ m30, m31, m32, m33 }
    );
}

- (void) updatePhongLoadAction
{
    CTX_LOG(@"MetalContext.updatePhongLoadAction()");
    phongRPD.colorAttachments[0].loadAction = MTLLoadActionLoad;
    rttCleared = true;
}

- (void) resetRenderPass
{
    CTX_LOG(@"MetalContext.resetRenderPass()");
    rttPassDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
    rttCleared = true;
}

- (void) clearRTT:(int)color red:(float)red green:(float)green blue:(float)blue alpha:(float)alpha clearDepth:(bool)clearDepth ignoreScissor:(bool)ignoreScissor
{
    CTX_LOG(@">>>> MetalContext.clearRTT() %f %f %f %f", red, green, blue, alpha);
    if (ignoreScissor && !isScissorRectSet) {
        CTX_LOG(@"     MetalContext.clearRTT()     clearing whole rtt");
        rttPassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
        rttPassDesc.colorAttachments[0].clearColor = MTLClearColorMake(red, green, blue, alpha);
    } else if (isScissorRectSet) {
        CTX_LOG(@"     MetalContext.clearRTT() scissorRect.x = %lu, scissorRect.y = %lu, scissorRect.width = %lu, scissorRect.height = %lu, color = %u",
                        scissorRect.x, scissorRect.y, scissorRect.width, scissorRect.height, color);

        MTLRegion region = MTLRegionMake2D(scissorRect.x, scissorRect.y,
                                            scissorRect.width, scissorRect.height);
        CTX_LOG(@"     MetalContext.clearRTT() %lu , %lu", [rtt getTexture].width, [rtt getTexture].height);

        if (rttClearColor != color) {
            rttClearColor = color;
            int* pixels = [clearBuffer contents];
            int length = [rtt getTexture].width * [rtt getTexture].height;
            for (int i = 0; i < length; i++) {
                *pixels++ = rttClearColor;
            }
        }

        // TODO: MTL: replaceRegion is a temporary fix for clearing the clipRect in rtt.
        // It should be changed to drawing a rect of size scissorRect, with all vertices
        // having rttClearColor as color. and should clear the depth buffer too.
        // When removing this code, clearBuffer should be removed.
        [[rtt getTexture] replaceRegion:region
                            mipmapLevel:0
                              withBytes:[clearBuffer contents]
                            bytesPerRow:(scissorRect.width * 4)];
    }
    CTX_LOG(@"<<<< MetalContext.clearRTT()");
}

- (void) setClipRect:(int)x y:(int)y width:(int)width height:(int)height
{
    CTX_LOG(@">>>> MetalContext.setClipRect()");
    id<MTLTexture> currRtt = [rtt getTexture];
    if (x <= 0 && y <= 0 && width >= currRtt.width && height >= currRtt.height) {
        CTX_LOG(@"     MetalContext.setClipRect() 1 resetting clip, %lu, %lu", currRtt.width, currRtt.height);
        [self resetClip];
    } else {
        CTX_LOG(@"     MetalContext.setClipRect() 2");
        if (x < 0)                        x = 0;
        if (y < 0)                        y = 0;
        if (width  > currRtt.width)  width  = currRtt.width;
        if (height > currRtt.height) height = currRtt.height;
        scissorRect.x = x;
        scissorRect.y = y;
        scissorRect.width  = width;
        scissorRect.height = height;
        isScissorRectSet = true;
    }
    CTX_LOG(@"<<<< MetalContext.setClipRect()");
}

- (void) resetClip
{
    CTX_LOG(@">>>> MetalContext.resetClip()");
    isScissorRectSet = false;
    scissorRect.x = 0;
    scissorRect.y = 0;
    scissorRect.width  = 0;
    scissorRect.height = 0;
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


    CTX_LOG(@"fillVB : numVerts = %d, numTriangles = %lu, numQuads = %d", numVerts, numTriangles, numQuads);

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
                pVert->color.w = ((float)(*(colors + 3)))/255.0f;

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

- (NSInteger) setDeviceParametersFor3D
{
    CTX_LOG(@"MetalContext_setDeviceParametersFor3D()");
    id<MTLCommandBuffer> commandBuffer = [self getCurrentCommandBuffer];
    phongRPD = [MTLRenderPassDescriptor new];
    if (!rttCleared) {
        phongRPD.colorAttachments[0].loadAction = MTLLoadActionClear;
    }
    phongRPD.colorAttachments[0].clearColor = MTLClearColorMake(1, 1, 1, 1); // make this programmable
    phongRPD.colorAttachments[0].storeAction = MTLStoreActionStore;
    phongRPD.colorAttachments[0].texture = [[self getRTT] getTexture];

    // TODO: MTL: Check whether we need to do shader initialization here
    /*if (!phongShader) {
        phongShader = ([[MetalPhongShader alloc] createPhongShader:self]);
    }*/
    return 1;
}

- (void) setCameraPosition:(float)x
            y:(float)y z:(float)z
{
    cPos.x = x;
    cPos.y = y;
    cPos.z = z;
    cPos.w = 0;
}

- (MTLRenderPassDescriptor*) getPhongRPD
{
    return phongRPD;
}

- (simd_float4x4) getMVPMatrix
{
    return mvpMatrix;
}

- (simd_float4x4) getWorldMatrix
{
    return worldMatrix;
}

- (vector_float4) getCameraPosition
{
    return cPos;
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

- (void)dealloc
{
    CTX_LOG(@">>>> MTLContext.dealloc -- releasing native MetalContext");

    if (commandQueue != nil) {
        [commandQueue release];
        commandQueue = nil;
    }

    if (pipelineManager != nil) {
        [pipelineManager dealloc];
        pipelineManager = nil;
    }

    if (rttPassDesc != nil) {
        [rttPassDesc release];
        rttPassDesc = nil;
    }

    if (sampler != nil) {
        [sampler release];
        sampler = nil;
    }

    if (phongShader != nil) {
        [phongShader release];
        phongShader = nil;
    }

    if (phongRPD != nil) {
        [phongRPD release];
        phongRPD = nil;
    }

    device = nil;

    [super dealloc];
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


JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nRelease
  (JNIEnv *env, jclass jClass, jlong context)
{
    CTX_LOG(@">>>> MTLContext_nRelease");

    MetalContext *contextPtr = jlong_to_ptr(context);

    if (contextPtr != NULL) {
        [contextPtr dealloc];
    }
    contextPtr = NULL;
    CTX_LOG(@"<<<< MTLContext_nRelease");
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

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetClipRect
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetClipRect
  (JNIEnv *env, jclass jClass, jlong ctx,
   jint x, jint y, jint width, jint height)
{
    CTX_LOG(@"MTLContext_nSetClipRect");
    MetalContext *pCtx = (MetalContext*)jlong_to_ptr(ctx);
    [pCtx setClipRect:x y:y width:width height:height];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nResetClipRect
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nResetClipRect
  (JNIEnv *env, jclass jClass, jlong ctx)
{
    CTX_LOG(@"MTLContext_nResetClipRect");
    MetalContext *pCtx = (MetalContext*)jlong_to_ptr(ctx);
    [pCtx resetClip];
}

JNIEXPORT jint JNICALL Java_com_sun_prism_mtl_MTLContext_nResetTransform
  (JNIEnv *env, jclass jClass, jlong context)
{
    CTX_LOG(@"MTLContext_nResetTransform");

    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);
    //[mtlContext resetProjViewMatrix];
    return 1;
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetSampler
  (JNIEnv *env, jclass jClass,
    jlong context, jboolean isLinear, jint wrapMode)
{
    CTX_LOG(@"MTLContext_nSetSampler");
    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);
    [mtlContext setSampler:isLinear  wrapMode:wrapMode];
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

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetWorldTransform
  (JNIEnv *env, jclass jClass,
    jlong context,
    jdouble m00, jdouble m01, jdouble m02, jdouble m03,
    jdouble m10, jdouble m11, jdouble m12, jdouble m13,
    jdouble m20, jdouble m21, jdouble m22, jdouble m23,
    jdouble m30, jdouble m31, jdouble m32, jdouble m33)
{
    CTX_LOG(@"MTLContext_nSetWorldTransform");
    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);

    CTX_LOG(@"%f %f %f %f", m00, m01, m02, m03);
    CTX_LOG(@"%f %f %f %f", m10, m11, m12, m13);
    CTX_LOG(@"%f %f %f %f", m20, m21, m22, m23);
    CTX_LOG(@"%f %f %f %f", m30, m31, m32, m33);

    [mtlContext setWorldTransformMatrix:m00 m01:m01 m02:m02 m03:m03
        m10:m10 m11:m11 m12:m12 m13:m13
        m20:m20 m21:m21 m22:m22 m23:m23
        m30:m30 m31:m31 m32:m32 m33:m33];

    return;
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetWorldTransformToIdentity
  (JNIEnv *env, jclass jClass,
    jlong context)
{
    CTX_LOG(@"MTLContext_nSetWorldTransformToIdentity");
    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);

    [mtlContext setWorldTransformIdentityMatrix:1 m01:0 m02:0 m03:0
        m10:0 m11:1 m12:0 m13:0
        m20:0 m21:0 m22:1 m23:0
        m30:0 m31:0 m32:0 m33:1];

    return;
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nCreateMTLMesh
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLContext_nCreateMTLMesh
  (JNIEnv *env, jclass jClass, jlong ctx)
{
    CTX_LOG(@"MTLContext_nCreateMTLMesh");
    //return 1;
    MetalContext *pCtx = (MetalContext*) jlong_to_ptr(ctx);

    MetalMesh* mesh = ([[MetalMesh alloc] createMesh:pCtx]);
    return ptr_to_jlong(mesh);
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nReleaseMTLMesh
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nReleaseMTLMesh
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMesh)
{
    // TODO: MTL: Complete the implementation
    CTX_LOG(@"MTLContext_nReleaseMTLMesh");
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nBuildNativeGeometryShort
 * Signature: (JJ[FI[SI)Z
 */
JNIEXPORT jboolean JNICALL Java_com_sun_prism_mtl_MTLContext_nBuildNativeGeometryShort
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMesh, jfloatArray vb, jint vbSize, jshortArray ib, jint ibSize)
{
    CTX_LOG(@"MTLContext_nBuildNativeGeometryShort");
    CTX_LOG(@"vbSize %d ibSize %d", vbSize, ibSize);
    MetalMesh *mesh = (MetalMesh *) jlong_to_ptr(nativeMesh);

    if (vbSize < 0 || ibSize < 0) {
        return JNI_FALSE;
    }

    unsigned int uvbSize = (unsigned int) vbSize;
    unsigned int uibSize = (unsigned int) ibSize;
    unsigned int vertexBufferSize = (*env)->GetArrayLength(env, vb);
    unsigned int indexBufferSize = (*env)->GetArrayLength(env, ib);
    CTX_LOG(@"vertexBufferSize %d indexBufferSize %d", vertexBufferSize, indexBufferSize);

    if (uvbSize > vertexBufferSize || uibSize > indexBufferSize) {
        return JNI_FALSE;
    }

    float *vertexBuffer = (float *) ((*env)->GetPrimitiveArrayCritical(env, vb, 0));
    if (vertexBuffer == NULL) {
        CTX_LOG(@"MTLContext_nBuildNativeGeometryShort vertexBuffer is NULL");
        return JNI_FALSE;
    }

    unsigned short *indexBuffer = (unsigned short *) ((*env)->GetPrimitiveArrayCritical(env, ib, 0));
    if (indexBuffer == NULL) {
        CTX_LOG(@"MTLContext_nBuildNativeGeometryShort indexBuffer is NULL");
        (*env)->ReleasePrimitiveArrayCritical(env, vb, vertexBuffer, 0);
        return JNI_FALSE;
    }

    bool result = [mesh buildBuffers:vertexBuffer
                                  vSize:uvbSize
                                iBuffer:indexBuffer
                                  iSize:uibSize];
    (*env)->ReleasePrimitiveArrayCritical(env, ib, indexBuffer, 0);
    (*env)->ReleasePrimitiveArrayCritical(env, vb, vertexBuffer, 0);

    return result;
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nBuildNativeGeometryInt
 * Signature: (JJ[FI[II)Z
 */
JNIEXPORT jboolean JNICALL Java_com_sun_prism_mtl_MTLContext_nBuildNativeGeometryInt
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMesh, jfloatArray vb, jint vbSize, jintArray ib, jint ibSize)
{
    // TODO: MTL: Complete the implementation
    CTX_LOG(@"MTLContext_nBuildNativeGeometryInt");
    return JNI_TRUE;
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nCreateMTLPhongMaterial
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLContext_nCreateMTLPhongMaterial
  (JNIEnv *env, jclass jClass, jlong ctx)
{
    CTX_LOG(@"MTLContext_nCreateMTLPhongMaterial");
    MetalContext *pCtx = (MetalContext*) jlong_to_ptr(ctx);

    MetalPhongMaterial *phongMaterial = ([[MetalPhongMaterial alloc] createPhongMaterial:pCtx]);
    return ptr_to_jlong(phongMaterial);
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nReleaseMTLPhongMaterial
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nReleaseMTLPhongMaterial
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativePhongMaterial)
{
    // TODO: MTL: Complete the implementation
    CTX_LOG(@"MTLContext_nReleaseMTLPhongMaterial");
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetDiffuseColor
 * Signature: (JJFFFF)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetDiffuseColor
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativePhongMaterial,
        jfloat r, jfloat g, jfloat b, jfloat a)
{
    CTX_LOG(@"MTLContext_nSetDiffuseColor");
    MetalPhongMaterial *phongMaterial = (MetalPhongMaterial *) jlong_to_ptr(nativePhongMaterial);
    [phongMaterial setDiffuseColor:r g:g b:b a:a];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetSpecularColor
 * Signature: (JJZFFFF)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetSpecularColor
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativePhongMaterial,
        jboolean set, jfloat r, jfloat g, jfloat b, jfloat a)
{
    CTX_LOG(@"MTLContext_nSetSpecularColor");
    MetalPhongMaterial *phongMaterial = (MetalPhongMaterial *) jlong_to_ptr(nativePhongMaterial);
    bool specularSet = set ? true : false;
    [phongMaterial setSpecularColor:specularSet r:r g:g b:b a:a];
}
/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetMap
 * Signature: (JJIJ)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetMap
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativePhongMaterial,
        jint mapType, jlong nativeTexture)
{
    // TODO: MTL: Complete the implementation
    CTX_LOG(@"MTLContext_nSetMap");
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nCreateMTLMeshView
 * Signature: (JJ)J
 */
JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLContext_nCreateMTLMeshView
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMesh)
{
    CTX_LOG(@"MTLContext_nCreateMTLMeshView");
    MetalContext *pCtx = (MetalContext*) jlong_to_ptr(ctx);

    MetalMesh *pMesh = (MetalMesh *) jlong_to_ptr(nativeMesh);

    MetalMeshView* meshView = ([[MetalMeshView alloc] createMeshView:pCtx
                                                                mesh:pMesh]);
    return ptr_to_jlong(meshView);
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nReleaseMTLMeshView
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nReleaseMTLMeshView
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMeshView)
{
    // TODO: MTL: Complete the implementation
    CTX_LOG(@"MTLContext_nReleaseMTLMeshView");
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetCullingMode
 * Signature: (JJI)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetCullingMode
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMeshView, jint cullMode)
{
    CTX_LOG(@"MTLContext_nSetCullingMode");
    MetalMeshView *meshView = (MetalMeshView *) jlong_to_ptr(nativeMeshView);

    switch (cullMode) {
        case com_sun_prism_mtl_MTLContext_CULL_BACK:
            cullMode = MTLCullModeBack;
            break;
        case com_sun_prism_mtl_MTLContext_CULL_FRONT:
            cullMode = MTLCullModeFront;
            break;
        case com_sun_prism_mtl_MTLContext_CULL_NONE:
            cullMode = MTLCullModeNone;
            break;
    }
    [meshView setCullingMode:cullMode];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetMaterial
 * Signature: (JJJ)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetMaterial
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMeshView, jlong nativePhongMaterial)
{
    CTX_LOG(@"MTLContext_nSetMaterial");
    MetalMeshView *meshView = (MetalMeshView *) jlong_to_ptr(nativeMeshView);

    MetalPhongMaterial *phongMaterial = (MetalPhongMaterial *) jlong_to_ptr(nativePhongMaterial);
    [meshView setMaterial:phongMaterial];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetWireframe
 * Signature: (JJZ)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetWireframe
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMeshView, jboolean wireframe)
{
    CTX_LOG(@"MTLContext_nSetWireframe");
    MetalMeshView *meshView = (MetalMeshView *) jlong_to_ptr(nativeMeshView);
    bool isWireFrame = wireframe ? true : false;
    [meshView setWireframe:isWireFrame];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetAmbientLight
 * Signature: (JJFFF)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetAmbientLight
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMeshView,
        jfloat r, jfloat g, jfloat b)
{
    CTX_LOG(@"MTLContext_nSetAmbientLight");
    MetalMeshView *meshView = (MetalMeshView *) jlong_to_ptr(nativeMeshView);
    [meshView setAmbientLight:r g:g b:b];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetLight
 * Signature: (JJIFFFFFFFFFFFFFFFFFF)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetLight
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMeshView, jint index,
        jfloat x, jfloat y, jfloat z, jfloat r, jfloat g, jfloat b, jfloat w,
        jfloat ca, jfloat la, jfloat qa, jfloat isAttenuated, jfloat range,
        jfloat dirX, jfloat dirY, jfloat dirZ, jfloat innerAngle, jfloat outerAngle, jfloat falloff)
{
    CTX_LOG(@"MTLContext_nSetLight");
    MetalMeshView *meshView = (MetalMeshView *) jlong_to_ptr(nativeMeshView);
    [meshView setLight:index
        x:x y:y z:z
        r:r g:g b:b w:w
        ca:ca la:la qa:qa
        isA:isAttenuated range:range
        dirX:dirX dirY:dirY dirZ:dirZ
        inA:innerAngle outA:outerAngle
        falloff:falloff];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nRenderMeshView
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nRenderMeshView
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMeshView)
{
    CTX_LOG(@"MTLContext_nRenderMeshView");
    MetalMeshView *meshView = (MetalMeshView *) jlong_to_ptr(nativeMeshView);
    [meshView render];
    return;
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetCameraPosition
 */

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetCameraPosition
  (JNIEnv *env, jclass jClass, jlong ctx, jdouble x,
   jdouble y, jdouble z)
{
    CTX_LOG(@"MTLContext_nSetDeviceParametersFor3D");
    MetalContext *pCtx = (MetalContext*)jlong_to_ptr(ctx);

    return [pCtx setCameraPosition:x
            y:y z:z];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetDeviceParametersFor3D
 */

JNIEXPORT jint JNICALL Java_com_sun_prism_mtl_MTLContext_nSetDeviceParametersFor3D
  (JNIEnv *env, jclass jClass, jlong ctx)
{
    CTX_LOG(@"MTLContext_nSetDeviceParametersFor3D");
    MetalContext *pCtx = (MetalContext*)jlong_to_ptr(ctx);

    return [pCtx setDeviceParametersFor3D];
}

/*
* Class:     com_sun_prism_mtl_MTLContext
* Method:    nSetCompositeMode
*/
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetCompositeMode(JNIEnv *env, jclass jClass, jlong context, jint mode)
{
    MetalContext* mtlCtx = (MetalContext*)jlong_to_ptr(context);

    MetalPipelineManager* pipeLineMgr = [mtlCtx getPipelineManager];

    [pipeLineMgr setCompositeBlendMode:mode];

    return;
}
