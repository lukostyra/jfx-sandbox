/*
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
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

#import "MetalMeshView.h"
#import "MetalPipelineManager.h"

#ifdef MESH_VERBOSE
#define MESH_LOG NSLog
#else
#define MESH_LOG(...)
#endif

@implementation MetalMeshView

- (MetalMeshView*) createMeshView:(MetalContext*)ctx
                             mesh:(MetalMesh*)mtlMesh
{
    self = [super init];
    if (self) {
        MESH_LOG(@"MetalMeshView_createMeshView()");
        context = ctx;
        mesh = mtlMesh;
        material = NULL;
        ambientLightColor.x = 0;
        ambientLightColor.y = 0;
        ambientLightColor.z = 0;
        ambientLightColor.w = 0;
        numLights = 0;
        lightsDirty = TRUE;
        cullMode = MTLCullModeNone;
        wireframe = FALSE;
    }
    return self;
}

- (void) setMaterial:(MetalPhongMaterial*)pMaterial
{
    MESH_LOG(@"MetalMeshView_setMaterial()");
    material = pMaterial;
}

- (void) setCullingMode:(int)cMode
{
    MESH_LOG(@"MetalMeshView_setCullingMode()");
    cullMode = cMode;
}

- (void) setWireframe:(bool)isWireFrame
{
    MESH_LOG(@"MetalMeshView_setWireframe()");
    wireframe = isWireFrame;
}

- (void) setAmbientLight:(float)r
                       g:(float)g
                       b:(float)b
{
    MESH_LOG(@"MetalMeshView_setAmbientLight()");
    ambientLightColor.x = r;
    ambientLightColor.y = g;
    ambientLightColor.z = b;
    ambientLightColor.w = 1;
}

- (void) computeNumLights
{
    MESH_LOG(@"MetalMeshView_scomputeNumLights()");
    if (!lightsDirty)
        return;
    lightsDirty = false;

    int n = 0;
    for (int i = 0; i != MAX_NUM_LIGHTS; ++i) {
        n += lights[i]->lightOn ? 1 : 0;
    }

    numLights = n;
}

- (void) setLight:(int)index
        x:(float)x y:(float)y z:(float)z
        r:(float)r g:(float)g b:(float)b w:(float)w
        ca:(float)ca la:(float)la qa:(float)qa
        isA:(float)isAttenuated range:(float)range
        dirX:(float)dirX dirY:(float)dirY dirZ:(float)dirZ
        inA:(float)innerAngle outA:(float)outerAngle
        falloff:(float)falloff
{
    MESH_LOG(@"MetalMeshView_setLight()");
    // NOTE: We only support up to 3 point lights at the present
    if (index >= 0 && index <= MAX_NUM_LIGHTS - 1) {
        MetalLight* light = ([[MetalLight alloc] createLight:x y:y z:z
            r:r g:g b:b w:w
            ca:ca la:la qa:qa
            isA:isAttenuated range:range
            dirX:dirX dirY:dirY dirZ:dirZ
            inA:innerAngle outA:outerAngle
            falloff:falloff]);
        lights[index] = light;
        lightsDirty = TRUE;
    }
}

- (MetalMesh*) getMesh
{
    return mesh;
}

- (int) getCullingMode
{
    return cullMode;
}

- (void) render
{
    [self computeNumLights];
    VS_PHONG_UNIFORMS vsUniforms;
    PS_PHONG_UNIFORMS psUniforms;
    for (int i = 0, d = 0, p = 0, c = 0, a = 0, r = 0, s = 0; i < MAX_NUM_LIGHTS; i++) {
        MetalLight* light = lights[i];

        vsUniforms.lightsPosition[p++] = light->position[0];
        vsUniforms.lightsPosition[p++] = light->position[1];
        vsUniforms.lightsPosition[p++] = light->position[2];
        vsUniforms.lightsPosition[p++] = 0;

        vsUniforms.lightsNormDirection[d++] = light->direction[0];
        vsUniforms.lightsNormDirection[d++] = light->direction[1];
        vsUniforms.lightsNormDirection[d++] = light->direction[2];
        vsUniforms.lightsNormDirection[d++] = 0;

        psUniforms.lightsColor[c++] = light->color[0];
        psUniforms.lightsColor[c++] = light->color[1];
        psUniforms.lightsColor[c++] = light->color[2];
        psUniforms.lightsColor[c++] = 1;

        psUniforms.lightsAttenuation[a++] = light->attenuation[0];
        psUniforms.lightsAttenuation[a++] = light->attenuation[1];
        psUniforms.lightsAttenuation[a++] = light->attenuation[2];
        psUniforms.lightsAttenuation[a++] = light->attenuation[3];

        psUniforms.lightsRange[r++] = light->maxRange;
        psUniforms.lightsRange[r++] = 0;
        psUniforms.lightsRange[r++] = 0;
        psUniforms.lightsRange[r++] = 0;

        if ([light isPointLight] || [light isDirectionalLight]) {
            psUniforms.spotLightsFactors[s++] = -1; // cos(180)
            psUniforms.spotLightsFactors[s++] = 2;  // cos(0) - cos(180)
            psUniforms.spotLightsFactors[s++] = 0;
            psUniforms.spotLightsFactors[s++] = 0;
        } else {
            // preparing for: I = pow((cosAngle - cosOuter) / (cosInner - cosOuter), falloff)
            float cosInner = cos(light->inAngle * M_PI / 180);
            float cosOuter = cos(light->outAngle * M_PI / 180);
            psUniforms.spotLightsFactors[s++] = cosOuter;
            psUniforms.spotLightsFactors[s++] = cosInner - cosOuter;
            psUniforms.spotLightsFactors[s++] = light->foff;
            psUniforms.spotLightsFactors[s++] = 0;
        }
    }

    id<MTLCommandBuffer> commandBuffer = [context getCurrentCommandBuffer];
    MTLRenderPassDescriptor* phongRPD = [context getPhongRPD];
    id<MTLRenderCommandEncoder> phongEncoder = [commandBuffer renderCommandEncoderWithDescriptor:phongRPD];
    id<MTLRenderPipelineState> phongPipelineState =
        [[context getPipelineManager] getPhongPipeStateWithFragFuncName:@"PhongPS"];
    [phongEncoder setRenderPipelineState:phongPipelineState];
    id<MTLDepthStencilState> depthStencilState =
        [[context getPipelineManager] getDepthStencilState];
    [phongEncoder setDepthStencilState:depthStencilState];
    // In Metal default winding order is Clockwise but the vertex data that
    // we are getting is in CounterClockWise order, so we need to set
    // MTLWindingCounterClockwise explicitly
    [phongEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [phongEncoder setCullMode:cullMode];
    MESH_LOG(@"MetalMeshView_render() wireframe : %d", wireframe);
    if (wireframe) {
        [phongEncoder setTriangleFillMode:MTLTriangleFillModeLines];
    }
    if ([context isScissorEnabled]) {
        [phongEncoder setScissorRect:[context getScissorRect]];
    }
    vsUniforms.mvp_matrix = [context getMVPMatrix];
    vsUniforms.world_matrix = [context getWorldMatrix];
    vsUniforms.cameraPos = [context getCameraPosition];
    vsUniforms.numLights = numLights;
    id<MTLBuffer> vBuffer = [mesh getVertexBuffer];
    [phongEncoder setVertexBuffer:vBuffer
                           offset:0
                            atIndex:0];
    [phongEncoder setVertexBytes:&vsUniforms
                               length:sizeof(vsUniforms)
                              atIndex:1];
    psUniforms.diffuseColor = [material getDiffuseColor];
    psUniforms.ambientLightColor = ambientLightColor;

    if ([material isSpecularColor]) {
        psUniforms.isSpecColor = true;
        psUniforms.specColor = [material getSpecularColor];
    } else {
        psUniforms.isSpecColor = false;
    }
    psUniforms.isSpecMap = [material isSpecularMap] ? true : false;
    psUniforms.isBumpMap = [material isBumpMap] ? true : false;
    psUniforms.isIlluminated = [material isSelfIllumMap] ? true : false;

    [phongEncoder setFragmentBytes:&psUniforms
                                length:sizeof(psUniforms)
                                atIndex:0];
    [phongEncoder setFragmentTexture:[material getMap:DIFFUSE]
                             atIndex:0];
    [phongEncoder setFragmentTexture:[material getMap:SPECULAR]
                             atIndex:1];
    [phongEncoder setFragmentTexture:[material getMap:BUMP]
                             atIndex:2];
    [phongEncoder setFragmentTexture:[material getMap:SELFILLUMINATION]
                             atIndex:3];

    [phongEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
        indexCount:[mesh getNumIndices]
        indexType:MTLIndexTypeUInt16
        indexBuffer:[mesh getIndexBuffer]
        indexBufferOffset:0];
    [phongEncoder endEncoding];

    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
    [context updatePhongLoadAction];
}

@end // MetalMeshView
