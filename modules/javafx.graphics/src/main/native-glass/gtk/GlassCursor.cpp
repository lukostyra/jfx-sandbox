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

    g_print("CREATE CURSOR--------------->\n");
    Cursor cursor = 0;

    cairo_surface_t* src_image;
    env->CallVoidMethod(pixels, jPixelsAttachData, PTR_TO_JLONG(&src_image));

    if (!EXCEPTION_OCCURED(env)) {
        Pixmap pixmap, mask_pixmap;
        XColor black;
	    black.red = black.green = black.blue = 0;

        int width, height, depth;
        width = cairo_xlib_surface_get_width(src_image);
        height = cairo_xlib_surface_get_height(src_image);
        depth = cairo_xlib_surface_get_depth(src_image);

        guint rowstride, data_stride, i, j;
        guint8 *data, *mask_data, *pixels;

        pixels = cairo_image_surface_get_data(src_image);
        cairo_surface_destroy(src_image);
        rowstride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
        data_stride = 4 * ((width + 31) / 32);
        data = g_new0(guint8, data_stride * height);
        mask_data = g_new0(guint8, data_stride * height);

        for (j = 0; j < height; j++) {
            guint8 *src = pixels + j * rowstride;
            guint8 *d = data + data_stride * j;
            guint8 *md = mask_data + data_stride * j;

            for (i = 0; i < width; i++) {
                if (src[1] < 0x80) {
                    *d |= 1 << (i % 8);
                }

                if (src[3] >= 0x80) {
                    *md |= 1 << (i % 8);
                }

                src += 4;
                if (i % 8 == 7) {
                    d++;
                    md++;
                }
            }
        }

        pixmap = XCreatePixmap(X_CURRENT_DISPLAY, DefaultRootWindow(X_CURRENT_DISPLAY), width, height, depth);
        mask_pixmap = XCreatePixmap(X_CURRENT_DISPLAY, DefaultRootWindow(X_CURRENT_DISPLAY), width, height, depth);

        cairo_surface_t* pm_sfc = cairo_xlib_surface_create_for_bitmap(X_CURRENT_DISPLAY,
                                                                       pixmap,
                                                                       DefaultScreenOfDisplay(X_CURRENT_DISPLAY),
                                                                       width, height);

        cairo_surface_t *image, *mask;
        cairo_t *cr;

        image = cairo_image_surface_create_for_data (data, CAIRO_FORMAT_A1, width, height, data_stride);
        cr = cairo_create(pm_sfc);
        cairo_set_source_surface(cr, image, 0, 0);
        cairo_surface_destroy(image);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_paint(cr);
        cairo_destroy(cr);


        mask = cairo_xlib_surface_create_for_bitmap(X_CURRENT_DISPLAY,
                                                   mask_pixmap,
                                                   DefaultScreenOfDisplay(X_CURRENT_DISPLAY),
                                                   width, height);
        cr = cairo_create(mask);
        image = cairo_image_surface_create_for_data(mask_data, CAIRO_FORMAT_A1, width, height, data_stride);
        cairo_set_source_surface(cr, image, 0, 0);
        cairo_surface_destroy(image);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_paint(cr);
        cairo_destroy(cr);


        cursor = XCreatePixmapCursor(X_CURRENT_DISPLAY,
                                    cairo_xlib_surface_get_drawable(pm_sfc),
                                    cairo_xlib_surface_get_drawable(mask),
                                    &black, &black, x, y);

        cairo_surface_destroy(pm_sfc);
        cairo_surface_destroy(mask);

        g_free(data);
        g_free(mask_data);

//        XFreePixmap(X_CURRENT_DISPLAY, pixmap);
//        XFreePixmap(X_CURRENT_DISPLAY, mask_pixmap);

        g_print("Created Cursor: %ld\n", cursor);
    } else {
        cairo_surface_destroy(src_image);
    }

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
