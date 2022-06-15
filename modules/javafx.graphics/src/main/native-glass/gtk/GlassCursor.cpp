/*
 * Copyright (c) 2011, 2014, Oracle and/or its affiliates. All rights reserved.
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

#include <cairo/cairo-xlib.h>
#include <X11/Xcursor/Xcursor.h>
#include <stdlib.h>
#include <jni.h>

#include "com_sun_glass_ui_Cursor.h"
#include "glass_general.h"
static Cursor emptyCursor = 0;
static Cursor get_empty_cursor() {
    if (emptyCursor == 0) {
        XColor color;
        char data[1];
        Pixmap pixmap = XCreateBitmapFromData(main_ctx->display, DefaultRootWindow(main_ctx->display), data, 1, 1);
        emptyCursor = XCreatePixmapCursor(main_ctx->display, pixmap, pixmap, &color, &color, 0, 0);

        XFreePixmap(main_ctx->display, pixmap);
    }

    return emptyCursor;
}

//See: https://www.freedesktop.org/wiki/Specifications/cursor-spec/
Cursor get_native_cursor(int type) {
    Cursor cursor;
    switch (type) {
        case com_sun_glass_ui_Cursor_CURSOR_TEXT:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "text");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_CROSSHAIR:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "crosshair");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_CLOSED_HAND:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "grabbing");
            if (cursor == 0) {
                cursor = XcursorLibraryLoadCursor(main_ctx->display, "fleur");
            }
            break;
            case com_sun_glass_ui_Cursor_CURSOR_OPEN_HAND:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "hand1");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_POINTING_HAND:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "pointer");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_UP:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "n-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_DOWN:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "s-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_UPDOWN:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "ns-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_LEFT:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "w-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_RIGHT:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "e-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_LEFTRIGHT:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "ew-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_SOUTHWEST:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "sw-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_NORTHEAST:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "ne-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_SOUTHEAST:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "se-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_NORTHWEST:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "nw-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_MOVE:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "move");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_WAIT:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "watch");
            if (cursor == 0) {
                cursor = XcursorLibraryLoadCursor(main_ctx->display, "progress");
            }
            break;
        case com_sun_glass_ui_Cursor_CURSOR_DISAPPEAR:
        case com_sun_glass_ui_Cursor_CURSOR_NONE:
            cursor = get_empty_cursor();
            break;
        case com_sun_glass_ui_Cursor_CURSOR_DEFAULT:
        default:
            cursor = XcursorLibraryLoadCursor(main_ctx->display, "default");
            break;
    }

    g_print("get_native_cursor: %ld\n", cursor);
    return cursor;
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

    g_print("CREATE CURSOR--------------->\n");
    Cursor cursor = 0;

    cairo_surface_t* src_image;
    env->CallVoidMethod(pixels, jPixelsAttachData, PTR_TO_JLONG(&src_image));

    if (!EXCEPTION_OCCURED(env)) {
        XcursorImage *xcimage;

        xcimage = XcursorImageCreate(cairo_image_surface_get_width(src_image),  cairo_image_surface_get_height(src_image));
        xcimage->xhot = x;
        xcimage->yhot = y;
        xcimage->pixels = (XcursorPixel *) cairo_image_surface_get_data(src_image);

        cursor = XcursorImageLoadCursor(main_ctx->display, xcimage);
        g_print("Created Cursor: %ld\n", cursor);
        XcursorImageDestroy(xcimage);
    }

    cairo_surface_destroy(src_image);

    return cursor;
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

    int size = XcursorGetDefaultSize(main_ctx->display);

    jclass jc = env->FindClass("com/sun/glass/ui/Size");
    if (env->ExceptionCheck()) return NULL;
    jobject jo = env->NewObject(
            jc,
            jSizeInit,
            size,
            size);
    EXCEPTION_OCCURED(env);
    return jo;
}

} // extern "C"
