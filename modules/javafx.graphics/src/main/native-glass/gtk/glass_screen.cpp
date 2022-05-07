/*
 * Copyright (c) 2013, 2021, Oracle and/or its affiliates. All rights reserved.
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

#include <stdlib.h>
#include "glass_screen.h"
#include "glass_general.h"

#include <X11/Xatom.h>


jfloat OverrideUIScale = -1.0f;
int DEFAULT_DPI = 96;

static guint get_current_desktop(Screen *screen) {
    Atom currentDesktopAtom = XInternAtom(X_CURRENT_DISPLAY, "_NET_CURRENT_DESKTOP", True);
    guint ret = 0;

    Atom type;
    int format;
    gulong num, left;
    unsigned long *data = NULL;

    if (currentDesktopAtom == None) {
        return 0;
    }

    int result = XGetWindowProperty(X_CURRENT_DISPLAY,
                                    RootWindowOfScreen(screen),
                                    currentDesktopAtom, 0, G_MAXLONG, False, XA_CARDINAL,
                                    &type, &format, &num, &left, (unsigned char **)&data);

    if ((result == Success) && (data != NULL)) {
        if (type == XA_CARDINAL && format == 32) {
            ret = data[0];
        }

        XFree(data);
    }

    return ret;

}

static XRectangle get_screen_workarea(Screen *screen) {
    XRectangle ret = { 0, 0, WidthOfScreen(screen), HeightOfScreen(screen) };

    //TODO: Window manager might not support _NET_WORKAREA
    Atom workareaAtom = XInternAtom(X_CURRENT_DISPLAY, "_NET_WORKAREA", True);

    Atom type;
    int format;
    gulong num, left;
    unsigned long *data = NULL;

    if (workareaAtom == None) {
        return ret;
    }

    int result = XGetWindowProperty(X_CURRENT_DISPLAY,
                                    RootWindowOfScreen(screen),
                                    workareaAtom, 0, G_MAXLONG, False, AnyPropertyType,
                                    &type, &format, &num, &left, (unsigned char **)&data);

    if ((result == Success) && (data != NULL)) {
        if (type != None && format == 32) {
            guint current_desktop = get_current_desktop(screen);
            if (current_desktop < num / 4) {
                ret.x = data[current_desktop * 4];
                ret.y = data[current_desktop * 4 + 1];
                ret.width = data[current_desktop * 4 + 2];
                ret.height = data[current_desktop * 4 + 3];
            }
        }

        XFree(data);
    }

    return ret;
}

jfloat getUIScale(Screen* screen) {
    return 1.0f;
//    jfloat uiScale;
//    if (OverrideUIScale > 0.0f) {
//        uiScale = OverrideUIScale;
//    } else {
//        char *scale_str = getenv("GDK_SCALE");
//        int gdk_scale = (scale_str == NULL) ? -1 : atoi(scale_str);
//        if (gdk_scale > 0) {
//            uiScale = (jfloat) gdk_scale;
//        } else {
//            uiScale = (jfloat) glass_settings_get_guint_opt("org.gnome.desktop.interface",
//                                                            "scaling-factor", 0);
//            if (uiScale < 1) {
//                uiScale = (jfloat) (gdk_screen_get_resolution(screen) / DEFAULT_DPI);
//            }
//            if (uiScale < 1) {
//                uiScale = 1;
//            }
//        }
//    }
//    return uiScale;
}

static jobject createJavaScreen(JNIEnv* env, Screen* screen) {

    XRectangle workArea = get_screen_workarea(screen);
    LOG4("Work Area: x:%d, y:%d, w:%d, h:%d\n", workArea.x, workArea.y, workArea.width, workArea.height);
    g_print("Work Area: x:%d, y:%d, w:%d, h:%d\n", workArea.x, workArea.y, workArea.width, workArea.height);

    XWindowAttributes rootwin_geometry;
    XGetWindowAttributes(X_CURRENT_DISPLAY, RootWindowOfScreen(screen), &rootwin_geometry);

//    LOG1("convert monitor[%d] -> glass Screen\n", monitor_idx)
    LOG4("[x: %d y: %d w: %d h: %d]\n",
         rootwin_geometry.x, rootwin_geometry.y,
         rootwin_geometry.width, rootwin_geometry.height)

    g_print("[x: %d y: %d w: %d h: %d]\n",
         rootwin_geometry.x, rootwin_geometry.y,
         rootwin_geometry.width, rootwin_geometry.height);

    jfloat uiScale = getUIScale(screen);

    jint mx = rootwin_geometry.x / uiScale;
    jint my = rootwin_geometry.y / uiScale;
    jint mw = rootwin_geometry.width / uiScale;
    jint mh = rootwin_geometry.height / uiScale;

    jint wx = workArea.x / uiScale;
    jint wy = workArea.y / uiScale;
    jint ww = workArea.width / uiScale;
    jint wh = workArea.height / uiScale;

    int mmW = WidthMMOfScreen(screen);
    int mmH = HeightMMOfScreen(screen);

    jint dpiX, dpiY;
    if (mmW <= 0 || mmH <= 0) {
        dpiX = dpiY = 96;
    } else {
        dpiX = (mw * 254) / (mmW * 10);
        dpiY = (mh * 254) / (mmH * 10);
    }

    jobject jScreen = env->NewObject(jScreenCls, jScreenInit,
                                     (jlong)XScreenNumberOfScreen(screen),

                                     DefaultDepthOfScreen(screen),

                                     mx, my, mw, mh,

                                     rootwin_geometry.x,
                                     rootwin_geometry.y,
                                     rootwin_geometry.width,
                                     rootwin_geometry.height,

                                     wx, wy, ww, wh,

                                     dpiX, dpiY,
                                     uiScale, uiScale, uiScale, uiScale);

    JNI_EXCEPTION_TO_CPP(env);
    return jScreen;
}

jobject createJavaScreen(JNIEnv* env, int monitor_idx) {
    Screen* default_screen = ScreenOfDisplay(X_CURRENT_DISPLAY, monitor_idx);

    try {
        return createJavaScreen(env, default_screen);
    } catch (jni_exception&) {
        return NULL;
    }
}

jobjectArray rebuild_screens(JNIEnv* env) {
    int n_monitors = ScreenCount(X_CURRENT_DISPLAY);

    jobjectArray jscreens = env->NewObjectArray(n_monitors, jScreenCls, NULL);
    JNI_EXCEPTION_TO_CPP(env)
    LOG1("Available screens: %d\n", n_monitors)

    int i;
    for (i=0; i < n_monitors; i++) {
        env->SetObjectArrayElement(jscreens, i, createJavaScreen(env, ScreenOfDisplay(X_CURRENT_DISPLAY, i)));
        JNI_EXCEPTION_TO_CPP(env)
    }

    return jscreens;
}

//
//glong getScreenPtrForLocation(int x, int y) {
//    //Note: we are relying on the fact that javafx_screen_id == gdk_monitor_id
//    return gdk_screen_get_monitor_at_point(gdk_screen_get_default(), x, y);
//}
