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
        Pixmap pixmap = XCreateBitmapFromData(X_CURRENT_DISPLAY, DefaultRootWindow(X_CURRENT_DISPLAY), data, 1, 1);
        emptyCursor = XCreatePixmapCursor(X_CURRENT_DISPLAY, pixmap, pixmap, &color, &color, 0, 0);

        XFreePixmap(X_CURRENT_DISPLAY, pixmap);
    }

    return emptyCursor;
}

//See: https://www.freedesktop.org/wiki/Specifications/cursor-spec/
Cursor get_native_cursor(int type) {
    Cursor cursor;
    switch (type) {
        case com_sun_glass_ui_Cursor_CURSOR_TEXT:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "text");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_CROSSHAIR:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "crosshair");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_CLOSED_HAND:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "grabbing");
            if (cursor == 0) {
                cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "fleur");
            }
            break;
            case com_sun_glass_ui_Cursor_CURSOR_OPEN_HAND:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "hand1");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_POINTING_HAND:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "pointer");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_UP:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "n-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_DOWN:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "s-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_UPDOWN:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "ns-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_LEFT:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "w-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_RIGHT:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "e-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_LEFTRIGHT:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "ew-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_SOUTHWEST:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "sw-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_NORTHEAST:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "ne-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_SOUTHEAST:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "se-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_RESIZE_NORTHWEST:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "nw-resize");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_MOVE:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "move");
            break;
        case com_sun_glass_ui_Cursor_CURSOR_WAIT:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "watch");
            if (cursor == 0) {
                cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "progress");
            }
            break;
        case com_sun_glass_ui_Cursor_CURSOR_DISAPPEAR:
        case com_sun_glass_ui_Cursor_CURSOR_NONE:
            cursor = get_empty_cursor();
            break;
        case com_sun_glass_ui_Cursor_CURSOR_DEFAULT:
        default:
            cursor = XcursorLibraryLoadCursor(X_CURRENT_DISPLAY, "default");
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

    Cursor cursor = 0;

    cairo_surface_t* img_surface;
    env->CallVoidMethod(pixels, jPixelsAttachData, PTR_TO_JLONG(&img_surface));

    if (!EXCEPTION_OCCURED(env)) {
        XColor color;
        Pixmap pixmap;

        int w, h, depth;
        w = cairo_xlib_surface_get_width(img_surface);
        h = cairo_xlib_surface_get_height(img_surface);
        depth = cairo_xlib_surface_get_depth(img_surface);

        pixmap = XCreatePixmap(X_CURRENT_DISPLAY, DefaultRootWindow(X_CURRENT_DISPLAY), w, h, depth);

        cairo_surface_t* x11_surface = cairo_xlib_surface_create_for_bitmap(X_CURRENT_DISPLAY,
                                                                            pixmap,
                                                                            DefaultScreenOfDisplay(X_CURRENT_DISPLAY),
                                                                            w, h);

        cairo_t *context = cairo_create(x11_surface);

        cairo_set_source_surface(context, img_surface, 0, 0);
        cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
        cairo_paint(context);
        cairo_destroy(context);
        cursor = XCreatePixmapCursor(X_CURRENT_DISPLAY, pixmap, pixmap, &color, &color, x, y);
        cairo_surface_destroy(x11_surface);
        XFreePixmap(X_CURRENT_DISPLAY, pixmap);
    }

    cairo_surface_destroy(img_surface);

    return PTR_TO_JLONG(&cursor);
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

    int size = XcursorGetDefaultSize(X_CURRENT_DISPLAY);

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
