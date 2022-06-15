/*
 * Copyright (c) 2011, 2021, Oracle and/or its affiliates. All rights reserved.
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

#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/sync.h>
#include <X11/XKBlib.h>
#include <X11/Xresource.h>

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
#include "glass_dnd.h"
#include "glass_key.h"
#include "glass_window.h"
#include "glass_screen.h"
#include "xsettings-client.h"

static void process_events(GdkEvent*, gpointer);

JNIEnv* mainEnv; // Use only with main loop thread!!!

extern gboolean disableGrab;

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

static gboolean x11_event_source_prepare(GSource* source, gint* timeout);
static gboolean x11_event_source_check(GSource* source);
static gboolean x11_event_source_dispatch(GSource* source, GSourceFunc callback, gpointer user_data);

static GSourceFuncs x11_event_funcs = {
    x11_event_source_prepare,
    x11_event_source_check,
    x11_event_source_dispatch,
    NULL,
    NULL,
    NULL
};

static GMainLoop *mainLoop = NULL;

static int DoubleClickTime = -1;
static int DoubleClickDistance = -1;

static void setting_notify_cb (const char       *name,
                               XSettingsAction   action,
                               XSettingsSetting *setting,
                               void             *data) {
//    g_print("SETTING %s\n", name);

    switch (action) {
        case XSETTINGS_ACTION_NEW:
        case XSETTINGS_ACTION_CHANGED:
            if (strcmp(name, "Net/DoubleClickTime") == 0) {
                DoubleClickTime = setting->data.v_int;
                g_print("DoubleClickTime = %d\n", DoubleClickTime);
            } else if (strcmp(name, "Net/DoubleClickDistance") == 0) {
                DoubleClickDistance = setting->data.v_int;
                g_print("DoubleClickDistance = %d\n", DoubleClickDistance);
            }
            break;
        case XSETTINGS_ACTION_DELETED:
            break;
    }

    xsettings_setting_free(setting);
}

static void x11_monitor_events(GSource* source) {
    MainContext *m_ctx = ((MainContext *)source);

    m_ctx->dpy_pollfd = {
        XConnectionNumber(m_ctx->display),
        G_IO_IN | G_IO_HUP | G_IO_ERR,
        0
    };

    g_source_add_poll(source, &m_ctx->dpy_pollfd);

    g_source_set_priority(source, G_PRIORITY_HIGH_IDLE + 50);
    g_source_set_can_recurse(source, TRUE);
    g_source_attach(source, NULL);
}

static gboolean x11_event_source_prepare(GSource* source, gint* timeout) {
    *timeout = -1;
    return TRUE;
}

static gboolean x11_event_source_check(GSource* source) {
    return TRUE;
}

static gboolean x11_event_source_dispatch(GSource* source, GSourceFunc callback, gpointer data) {
    XEvent xevent;

    Display *display = main_ctx->display;
    WindowContext* ctx;

    while (XPending(display)) {
        XNextEvent(display, &xevent);

        if (xsettings_client_process_event(main_ctx->settings_client, &xevent)) {
            continue;
        }

        if (xevent.xany.window == DefaultRootWindow(display)) {
            g_print("============> Root Window Event %d\n", xevent.type);

            if (main_ctx->randr_event_base + RRScreenChangeNotify == xevent.type) {
                g_print("============> UPDATE SCREENS %d\n", xevent.type);
                mainEnv->CallStaticVoidMethod(jScreenCls, jScreenNotifySettingsChanged);
                LOG_EXCEPTION(mainEnv);
            }

            continue;
        }

        if (XFindContext(display, xevent.xany.window, X_CONTEXT, (XPointer *) &ctx) != 0) {
            //g_print("CTX not found: %d, win: %ld\n", X_CONTEXT, xevent.xany.window);
            continue;
        }

        if (ctx == NULL) {
            continue;
        }

        if (ctx->hasIME() && ctx->filterIME(&xevent)) {
            continue;
        }

        EventsCounterHelper helper(ctx);
        try {
            switch (xevent.type) {
                case VisibilityNotify:
                    ctx->process_visibility(&xevent.xvisibility);
                    break;
                case ConfigureNotify:
                    ctx->process_configure(&xevent.xconfigure);
                    break;
                case PropertyNotify:
                    ctx->process_property(&xevent.xproperty);
                    break;
                case MapNotify:
                    ctx->process_map();
                    break;
                case FocusIn:
                case FocusOut:
                    ctx->process_focus(&xevent.xfocus);
                    break;
                case Expose:
                    ctx->process_expose(&xevent.xexpose);
                    break;
                case XDamageNotify:
                    ctx->process_damage((XDamageNotifyEvent*)&xevent);
                    break;
                case KeyPress:
                case KeyRelease:
                    ctx->process_key(&xevent.xkey);
                    break;
                case ButtonPress:
                case ButtonRelease:
                    ctx->process_mouse_button(&xevent.xbutton);
                    break;
                case MotionNotify:
                    ctx->process_mouse_motion(&xevent.xmotion);
                    break;
                case EnterNotify:
                case LeaveNotify:
                    ctx->process_mouse_cross(&xevent.xcrossing);
                    break;
                case ClientMessage:
                    ctx->process_client_message(&xevent.xclient);
                    break;
                default:
                    XFilterEvent(&xevent, None);
            }
        } catch (jni_exception&) {
        }
    }

    return TRUE;
}

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

    GSource *source = g_source_new(&x11_event_funcs, sizeof(MainContext));
    main_ctx = ((MainContext *) source);

    Display *display = XOpenDisplay(NULL);
    main_ctx->display = display;

    int major, minor, ignore;

    //XrandR
    if (XRRQueryExtension(display, &main_ctx->randr_event_base, &ignore)) {
        XRRSelectInput(display, DefaultRootWindow(display), RRScreenChangeNotifyMask);
    }

    if (XSyncQueryExtension(display, &main_ctx->xsync_event_base, &ignore)) {
        XSyncInitialize(display, &major, &minor);
    }
    
    XDamageQueryExtension(display, &main_ctx->xdamage_event_base, &ignore);

    gint xkb_major = XkbMajorVersion;
    gint xkb_minor = XkbMinorVersion;

    if (XkbLibraryVersion(&xkb_major, &xkb_minor)) {
        xkb_major = XkbMajorVersion;
        xkb_minor = XkbMinorVersion;

        if (XkbQueryExtension(display, NULL, &main_ctx->xkb_event_type, NULL, &major, &minor)) {
            xkbAvailable = true;
            Bool detectable_autorepeat_supported;

            XkbSelectEvents(display,
                            XkbUseCoreKbd,
                            XkbNewKeyboardNotifyMask | XkbMapNotifyMask | XkbStateNotifyMask,
                            XkbNewKeyboardNotifyMask | XkbMapNotifyMask | XkbStateNotifyMask);

            XkbSelectEventDetails(display,
                                  XkbUseCoreKbd, XkbStateNotify,
                                  XkbAllStateComponentsMask,
                                  XkbModifierStateMask|XkbGroupStateMask);

            XkbSetDetectableAutoRepeat(display, True, &detectable_autorepeat_supported);
//        TODO: handle those events
        }
    }

    //TODO: can get setting change notification
    main_ctx->settings_client = xsettings_client_new(display, DefaultScreen(display), setting_notify_cb, NULL, NULL);

    x11_monitor_events(source);

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

//    glass_gdk_x11_display_set_window_scale(gdk_display_get_default(), 1);

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

    env->CallVoidMethod(launchable, jRunnableRun);
    CHECK_JNI_EXCEPTION(env);

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

    if (main_ctx) {
        g_print("g_source_destroy\n");
        g_source_remove_poll(&main_ctx->source, &main_ctx->dpy_pollfd);
        g_source_destroy(&main_ctx->source);
        g_print("xsettings_client_destroy\n");
        //xsettings_client_destroy(main_ctx->settings_client);
        g_print("XCloseDisplay\n");
        XCloseDisplay(main_ctx->display);
        g_free(main_ctx);
        main_ctx = NULL;
    }

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


//    g_print("submitForLaterInvocation\n");
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

    return DoubleClickTime;
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

    return DoubleClickDistance;
}

/*
 * Class:     com_sun_glass_ui_gtk_GtkApplication
 * Method:    staticView_getMultiClickMaxY
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_sun_glass_ui_gtk_GtkApplication_staticView_1getMultiClickMaxY
  (JNIEnv * env, jobject obj)
{
    return DoubleClickDistance;
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

    int ignore;

    return XCompositeQueryExtension(main_ctx->display, &ignore, &ignore);
}

} // extern "C"