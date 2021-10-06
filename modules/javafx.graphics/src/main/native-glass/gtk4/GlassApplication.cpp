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
#include <X11/Xlib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib.h>

#include <cstdlib>
#include <com_sun_glass_ui_gtk_GtkApplication.h>
#include <com_sun_glass_events_WindowEvent.h>
#include <com_sun_glass_events_MouseEvent.h>
#include <com_sun_glass_events_ViewEvent.h>
#include <com_sun_glass_events_KeyEvent.h>
#include <jni.h>

#include "glass_general.h"
#include "glass_evloop.h"
//#include "glass_dnd.h"
#include "glass_window.h"
#include "glass_screen.h"

JNIEnv* mainEnv; // Use only with main loop thread!!!

extern gboolean disableGrab;
static GMainLoop *mainLoop = NULL;

static gboolean call_runnable (gpointer data)
{
    RunnableContext* context = reinterpret_cast<RunnableContext*>(data);

    JNIEnv *env;
    int envStatus = javaVM->GetEnv((void **)&env, JNI_VERSION_1_6);
    if (envStatus == JNI_EDETACHED) {
        javaVM->AttachCurrentThread((void **)&env, NULL);
    }

    env->CallVoidMethod(context->runnable, jRunnableRun, NULL);
    LOG_EXCEPTION(env);
    env->DeleteGlobalRef(context->runnable);
    free(context);

    if (envStatus == JNI_EDETACHED) {
        javaVM->DetachCurrentThread();
    }

    return FALSE;
}


extern "C" {

jboolean gtk_verbose = JNI_FALSE;

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    _initGTK
 * Signature: (IZ)I
 */
JNIEXPORT jint JNICALL Java_com_sun_glass_ui_gtk_GtkApplication__1initGTK
  (JNIEnv *env, jclass clazz, jint version, jboolean verbose, jfloat uiScale)
{
    (void) clazz;
    (void) version;

    OverrideUIScale = uiScale;
    gtk_verbose = verbose;

    env->ExceptionClear();

    g_print("initGtk\n");

    gtk_init();

//    gchar * app_name = get_application_name();
//    g_print("initGtk %s\n", app_name);

//    app = gtk_application_new(NULL, G_APPLICATION_FLAGS_NONE);

//    if (app_name) {
//        g_free(app_name);
//    }

//    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
//    g_application_run(G_APPLICATION(app), 0, NULL);

    return JNI_TRUE;
}

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    _queryLibrary
 * Signature: Signature: (IZ)I
 */
#ifndef STATIC_BUILD
JNIEXPORT jint JNICALL Java_com_sun_glass_ui_gtk_GtkApplication__1queryLibrary
  (JNIEnv *env, jclass clazz, jint suggestedVersion, jboolean verbose)
{
    // If we are being called, then the launcher is
    // not in use, and we are in the proper glass library already.
    // This can be done by renaming the gtk versioned native
    // libraries to be libglass.so
    // Note: we will make no effort to complain if the suggestedVersion
    // is out of phase.

    (void)env;
    (void)clazz;
    (void)suggestedVersion;
    (void)verbose;

    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        return com_sun_glass_ui_gtk_GtkApplication_QUERY_NO_DISPLAY;
    }
    XCloseDisplay(display);

    return com_sun_glass_ui_gtk_GtkApplication_QUERY_USE_CURRENT;
}
#endif

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    _init
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_sun_glass_ui_gtk_GtkApplication__1init
  (JNIEnv * env, jobject obj, jlong handler, jboolean _disableGrab)
{
    (void)obj;

    mainEnv = env;
    disableGrab = (gboolean) _disableGrab;

    GdkDisplay *default_display = gdk_display_get_default();

    if (default_display != NULL) {
        g_signal_connect(G_OBJECT(default_display), "setting-changed",
                         G_CALLBACK(display_settings_changed), NULL);
    }

    g_print("inited\n");
}

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    _runLoop
 * Signature: (Ljava/lang/Runnable;Z)V
 */
JNIEXPORT void JNICALL Java_com_sun_glass_ui_gtk_GtkApplication__1runLoop
  (JNIEnv * env, jobject obj, jobject launchable, jboolean noErrorTrap)
{
    (void)obj;
    (void)noErrorTrap;

    mainEnv->CallVoidMethod(launchable, jRunnableRun);
    CHECK_JNI_EXCEPTION(mainEnv);

    mainLoop = g_main_loop_new(NULL, FALSE);
    g_print("run loop\n");
    g_main_loop_run(mainLoop);
    g_print("unref loop\n");
    g_main_loop_unref(mainLoop);
}

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    _terminateLoop
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_sun_glass_ui_gtk_GtkApplication__1terminateLoop
  (JNIEnv * env, jobject obj)
{
    (void)env;
    (void)obj;

    g_print("terminateLoop\n");
    g_main_loop_unref(mainLoop);
}

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    _submitForLaterInvocation
 * Signature: (Ljava/lang/Runnable;)V
 */
JNIEXPORT void JNICALL Java_com_sun_glass_ui_gtk_GtkApplication__1submitForLaterInvocation
  (JNIEnv * env, jobject obj, jobject runnable)
{
    (void)obj;

    g_print("submitForLaterInvocation\n");
    RunnableContext* context = (RunnableContext*)malloc(sizeof(RunnableContext));
    context->runnable = env->NewGlobalRef(runnable);
    g_idle_add_full(G_PRIORITY_HIGH_IDLE + 30, call_runnable, context, NULL);
}

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    enterNestedEventLoopImpl
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_sun_glass_ui_gtk_GtkApplication_enterNestedEventLoopImpl
  (JNIEnv * env, jobject obj)
{
    (void)env;
    (void)obj;

    g_print("enterNestedEventLoopImpl\n");
    g_main_loop_ref(mainLoop);
}

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    leaveNestedEventLoopImpl
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_sun_glass_ui_gtk_GtkApplication_leaveNestedEventLoopImpl
  (JNIEnv * env, jobject obj)
{
    (void)env;
    (void)obj;

    g_print("leaveNestedEventLoopImpl\n");
    g_main_loop_unref(mainLoop);
}

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    staticScreen_getScreens
 * Signature: ()[Lcom/sun/glass/ui/Screen;
 */
JNIEXPORT jobjectArray JNICALL Java_com_sun_glass_ui_gtk_GtkApplication_staticScreen_1getScreens
  (JNIEnv * env, jobject obj)
{
    (void)obj;

    try {
        return rebuild_screens(env);
    } catch (jni_exception&) {
        return NULL;
    }
}

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    staticTimer_getMinPeriod
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_sun_glass_ui_gtk_GtkApplication_staticTimer_1getMinPeriod
  (JNIEnv * env, jobject obj)
{
    (void)env;
    (void)obj;

    return 0; // There are no restrictions on period in g_threads
}

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    staticTimer_getMaxPeriod
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_sun_glass_ui_gtk_GtkApplication_staticTimer_1getMaxPeriod
  (JNIEnv * env, jobject obj)
{
    (void)env;
    (void)obj;

    return 10000; // There are no restrictions on period in g_threads
}

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    staticView_getMultiClickTime
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_com_sun_glass_ui_gtk_GtkApplication_staticView_1getMultiClickTime
  (JNIEnv * env, jobject obj)
{
    (void)env;
    (void)obj;

    static gint multi_click_time = -1;
    if (multi_click_time == -1) {
        g_object_get(gtk_settings_get_default(), "gtk-double-click-time", &multi_click_time, NULL);
    }
    return (jlong)multi_click_time;
}

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    staticView_getMultiClickMaxX
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_sun_glass_ui_gtk_GtkApplication_staticView_1getMultiClickMaxX
  (JNIEnv * env, jobject obj)
{
    (void)env;
    (void)obj;

    static gint multi_click_dist = -1;

    if (multi_click_dist == -1) {
        g_object_get(gtk_settings_get_default(), "gtk-double-click-distance", &multi_click_dist, NULL);
    }
    return multi_click_dist;
}

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    staticView_getMultiClickMaxY
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_sun_glass_ui_gtk_GtkApplication_staticView_1getMultiClickMaxY
  (JNIEnv * env, jobject obj)
{
    return Java_com_sun_glass_ui_gtk_GtkApplication_staticView_1getMultiClickMaxX(env, obj);
}

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    _supportsTransparentWindows
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_com_sun_glass_ui_gtk_GtkApplication__1supportsTransparentWindows
  (JNIEnv * env, jobject obj) {
    (void)env;
    (void)obj;

    return gdk_display_is_composited(gdk_display_get_default());
}

} // extern "C"
