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

import java.lang.reflect.Method;

/**
 * This class is used as a common entry point for generating Decora shaders.
 */

public class GenAllDecoraShaders {

    // TODO: MTL: This array should be passed from build.gradle
    static String [][] compileShaders = {
            {"CompileJSL", "-all", "ColorAdjust"},
            {"CompileJSL", "-all", "Brightpass"},
            {"CompileJSL", "-all", "SepiaTone"},
            {"CompileJSL", "-all", "PerspectiveTransform"},
            {"CompileJSL", "-all", "DisplacementMap"},
            {"CompileJSL", "-all", "InvertMask"},
            {"CompileBlend", "-all", "Blend"},
            {"CompilePhong", "-all", "PhongLighting"},
            {"CompileLinearConvolve", "-hw", "LinearConvolve"},
            {"CompileLinearConvolve", "-hw", "LinearConvolveShadow"}
    };

    /*
    // TODO: MTL: Cleanup
    // This commented block is copied from build.gradle. Only for reference to
    // compare how it was before introducing this file.

    [fileName: "ColorAdjust", generator: "CompileJSL", outputs: "-all"],
    [fileName: "Brightpass", generator: "CompileJSL", outputs: "-all"],
    [fileName: "SepiaTone", generator: "CompileJSL", outputs: "-all"],
    [fileName: "PerspectiveTransform", generator: "CompileJSL", outputs: "-all"],
    [fileName: "DisplacementMap", generator: "CompileJSL", outputs: "-all"],
    [fileName: "InvertMask", generator: "CompileJSL", outputs: "-all"],
    [fileName: "Blend", generator: "CompileBlend", outputs: "-all"],
    [fileName: "PhongLighting", generator: "CompilePhong", outputs: "-all"],
    [fileName: "LinearConvolve", generator: "CompileLinearConvolve", outputs: "-hw"],
    [fileName: "LinearConvolveShadow", generator: "CompileLinearConvolve", outputs: "-hw"]
    */

    public static void main(String[] args) throws Exception {
        /*
        // TODO: MTL: Cleanup
        System.err.println("GenAllDecoraShaders.main() -> following are the arguments received: ");
        for (int i = 0; i < args.length; i++) {
            System.err.println("                           args[" + i + "]  = " + args[i]);
        } */
        for (int i = 0; i < compileShaders.length; i++) {
            args[7] = compileShaders[i][1];
            args[8] = compileShaders[i][2];
            Class<?> cls = Class.forName(compileShaders[i][0]);
            Method meth = cls.getMethod("main", String[].class);
            meth.invoke(null, (Object) args);
        }
    }
}
