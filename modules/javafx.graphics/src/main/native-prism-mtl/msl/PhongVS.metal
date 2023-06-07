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

#include <metal_stdlib>
#include <simd/simd.h>
#include "PhongVSDecl.h"
#include "PhongVS2PS.h"
using namespace metal;

void quatToMatrix(float4 q, float3 N[3]) {
    float3 t1 = q.xyz * q.yzx * 2;
    float3 t2 = q.zxy * q.www * 2;
    float3 t3 = q.xyz * q.xyz * 2;
    float3 t4 = 1 - (t3 + t3.yzx);

    float3 r1 = t1 + t2;
    float3 r2 = t1 - t2;

    N[0] = float3(t4.y, r1.x, r2.z);
    N[1] = float3(r2.x, t4.z, r1.y);
    N[2] = float3(r1.z, r2.y, t4.x);

    N[2] *= (q.w >= 0) ? 1 : -1;   // ATI normal map generator compatibility
}

float3 getLocalVector(float3 global, float3 N[3]) {
    return float3(dot(global, N[1]), dot(global, N[2]), dot(global, N[0]));
}

vertex VS_PHONG_INOUT PhongVS(const uint v_id [[ vertex_id ]],
                      constant VS_PHONG_INPUT * v_in [[ buffer(0) ]],
                      constant VS_PHONG_UNIFORMS & vsUniforms [[ buffer(1) ]])
{
    VS_PHONG_INOUT out;
    out.texCoord = v_in[v_id].texCoord;
    out.numLights = vsUniforms.numLights;
    float4 worldVertexPos = vsUniforms.world_matrix * (float4(v_in[v_id].position, 1.0));

    out.position = vsUniforms.mvp_matrix * worldVertexPos;

    float3 n[3];
    quatToMatrix(v_in[v_id].normal, n);
    float3x3 sWorldMatrix = float3x3(vsUniforms.world_matrix[0].xyz,
                                     vsUniforms.world_matrix[1].xyz,
                                     vsUniforms.world_matrix[2].xyz);
    for (int i = 0; i != 3; ++i) {
        n[i] = (sWorldMatrix) * n[i];
    }

    float3 worldVecToEye = vsUniforms.cameraPos.xyz - worldVertexPos.xyz;
    out.worldVecToEye = getLocalVector(worldVecToEye, n);

    // TODO: MTL: Implementation using array of scalars
    // which is not working
    /*for (int i = 0; i < vsUniforms.numLights; i++) {
        float4 lightPos = float4(vsUniforms.lightsPosition[(i * 4)],
                                 vsUniforms.lightsPosition[(i * 4) + 1],
                                 vsUniforms.lightsPosition[(i * 4) + 2],
                                 0.0);
        float4 lightDir = float4(vsUniforms.lightsNormDirection[(i * 4)],
                                 vsUniforms.lightsNormDirection[(i * 4) + 1],
                                 vsUniforms.lightsNormDirection[(i * 4) + 2],
                                 0.0);
        float3 worldVecToLight = (lightPos).xyz - worldVertexPos.xyz;
        float3 worldVecToLightLocal = getLocalVector(worldVecToLight, n);
        out.worldVecsToLights[(i * 3)] = worldVecToLightLocal.x;
        out.worldVecsToLights[(i * 3) + 1] = worldVecToLightLocal.y;
        out.worldVecsToLights[(i * 3) + 2] = worldVecToLightLocal.z;

        float3 worldNormLightDir = (lightDir).xyz;
        float3 worldNormLightDirsLocal = getLocalVector(worldNormLightDir, n);
        out.worldNormLightDirs[(i * 3)] = worldNormLightDirsLocal.x;
        out.worldNormLightDirs[(i * 3) + 1] = worldNormLightDirsLocal.y;
        out.worldNormLightDirs[(i * 3) + 2] = worldNormLightDirsLocal.z;
    }*/
    float4 lightPos1 = float4(vsUniforms.lightsPosition[0],
                              vsUniforms.lightsPosition[1],
                              vsUniforms.lightsPosition[2],
                              0.0);
    float4 lightPos2 = float4(vsUniforms.lightsPosition[4],
                              vsUniforms.lightsPosition[5],
                              vsUniforms.lightsPosition[6],
                              0.0);
    float4 lightPos3 = float4(vsUniforms.lightsPosition[8],
                              vsUniforms.lightsPosition[9],
                              vsUniforms.lightsPosition[10],
                              0.0);
    float4 lightDir1 = float4(vsUniforms.lightsNormDirection[0],
                              vsUniforms.lightsNormDirection[1],
                              vsUniforms.lightsNormDirection[2],
                              0.0);
    float4 lightDir2 = float4(vsUniforms.lightsNormDirection[4],
                              vsUniforms.lightsNormDirection[5],
                              vsUniforms.lightsNormDirection[6],
                              0.0);
    float4 lightDir3 = float4(vsUniforms.lightsNormDirection[8],
                              vsUniforms.lightsNormDirection[9],
                              vsUniforms.lightsNormDirection[10],
                              0.0);
    float3 worldVecToLight = (lightPos1).xyz - worldVertexPos.xyz;
    out.worldVecsToLights1 = getLocalVector(worldVecToLight, n);
    float3 worldNormLightDir = (lightDir1).xyz;
    out.worldNormLightDirs1 = getLocalVector(worldNormLightDir, n);

    worldVecToLight = (lightPos2).xyz - worldVertexPos.xyz;
    out.worldVecsToLights2 = getLocalVector(worldVecToLight, n);
    worldNormLightDir = (lightDir2).xyz;
    out.worldNormLightDirs2 = getLocalVector(worldNormLightDir, n);

    worldVecToLight = (lightPos3).xyz - worldVertexPos.xyz;
    out.worldVecsToLights3 = getLocalVector(worldVecToLight, n);
    worldNormLightDir = (lightDir3).xyz;
    out.worldNormLightDirs3 = getLocalVector(worldNormLightDir, n);

    return out;
}

// TODO : Vertex shader with vertexdescriptor, we may need to use this in future,
// if not we can remove it
/*vertex VS_PHONG_INOUT PhongVS(VS_PHONG_INPUT in [[stage_in]],
                      constant float4x4 & mvp_matrix [[ buffer(1) ]],
                      constant float4x4 & world_matrix [[ buffer(2) ]])
{
    VS_PHONG_INOUT out;
    float4x4 mvpMatrix = mvp_matrix * world_matrix;
    out.position = mvpMatrix * in.position;
    return out;
}*/
