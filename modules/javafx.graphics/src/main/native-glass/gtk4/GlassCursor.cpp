/*
 * Copyright (c) 2011, 2020, Oracle and/or its affiliates. All rights reserved.
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
#include <com_sun_glass_ui_gtk_GtkCursor.h>

#include <gdk/gdk.h>
#include <stdlib.h>
#include <jni.h>

#include "com_sun_glass_ui_Cursor.h"
#include "glass_general.h"

GdkCursor* get_native_cursor(int type)
{
    gchar* cursor_name = NULL;

    GdkCursor *def_cur = gdk_cursor_new_from_name("default", NULL);

    switch (type) {
        case com_sun_glass_ui_Cursor_CURSOR_DEFAULT:
            return gdk_cursor_new_from_name("default", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_TEXT:
            return gdk_cursor_new_from_name("text", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_CROSSHAIR:
            return gdk_cursor_new_from_name("crosshair", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_CLOSED_HAND:
            return gdk_cursor_new_from_name("grabbing", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_OPEN_HAND:
            return gdk_cursor_new_from_name("grab", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_POINTING_HAND:
            return gdk_cursor_new_from_name("pointer", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_UP:
            return gdk_cursor_new_from_name("n-resize", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_DOWN:
            return gdk_cursor_new_from_name("s-resize", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_UPDOWN:
            return gdk_cursor_new_from_name("ns-resize", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_LEFT:
            return gdk_cursor_new_from_name("w-resize", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_RIGHT:
            return gdk_cursor_new_from_name("e-resize", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_LEFTRIGHT:
            return gdk_cursor_new_from_name("ew-resize", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_SOUTHWEST:
            return gdk_cursor_new_from_name("sw-resize", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_NORTHEAST:
            return gdk_cursor_new_from_name("ne-resize", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_SOUTHEAST:
            return gdk_cursor_new_from_name("se-resize", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_NORTHWEST:
            return gdk_cursor_new_from_name("nw-resize", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_MOVE:
            return gdk_cursor_new_from_name("move", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_WAIT:
            return gdk_cursor_new_from_name("wait", def_cur);
        case com_sun_glass_ui_Cursor_CURSOR_DISAPPEAR:
        case com_sun_glass_ui_Cursor_CURSOR_NONE:
            return gdk_cursor_new_from_name("none", def_cur);
        default:
            return def_cur;
    }

    return def_cur;
}

extern "C" {

/*
 * Class:     com_sun_glass_ui_gtk_GtkCursor
 * Method:    _createCursor
 * Signature: (IILcom/sun/glass/ui/Pixels;)J
 */
JNIEXPORT jlong JNICALL Java_com_sun_glass_ui_gtk_GtkCursor__1createCursor
  (JNIEnv * env, jobject obj, jint x, jint y, jobject pixels)
{
    (void)obj;

    GdkPixbuf *pixbuf = NULL;
    GdkCursor *cursor = NULL;
    env->CallVoidMethod(pixels, jPixelsAttachData, PTR_TO_JLONG(&pixbuf));
    if (!EXCEPTION_OCCURED(env)) {
        GdkTexture *texture = gdk_texture_new_for_pixbuf(pixbuf);
        //FIXME: fallback cursor?
        cursor = gdk_cursor_new_from_texture(texture, x, y, NULL);
        g_object_unref(texture);
    }
    g_object_unref(pixbuf);

    return PTR_TO_JLONG(cursor);
}

/*
 * Class:     com_sun_glass_ui_gtk_GtkCursor
 * Method:    _getBestSize
 * Signature: (II)Lcom.sun.glass.ui.Size
 */
JNIEXPORT jobject JNICALL Java_com_sun_glass_ui_gtk_GtkCursor__1getBestSize
        (JNIEnv *env, jclass jCursorClass, jint width, jint height)
{
    (void)jCursorClass;
    (void)width;
    (void)height;

//FIXME: not sure if below is equivalent
//    int size = gdk_display_get_default_cursor_size(gdk_display_get_default());

    int size;
    g_object_get(gtk_settings_get_default(), "gtk-double-click-tim", &size, NULL);

    jclass jc = env->FindClass("com/sun/glass/ui/Size");
    if (env->ExceptionCheck()) return NULL;
    jobject jo =  env->NewObject(
            jc,
            jSizeInit,
            size,
            size);
    EXCEPTION_OCCURED(env);
    return jo;
}

} // extern "C"
