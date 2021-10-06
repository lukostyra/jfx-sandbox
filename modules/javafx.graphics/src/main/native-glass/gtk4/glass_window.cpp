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
#include "glass_window.h"
#include "glass_key.h"
#include "glass_screen.h"
//#include "glass_dnd.h"

#include <com_sun_glass_events_WindowEvent.h>
#include <com_sun_glass_events_ViewEvent.h>
#include <com_sun_glass_events_MouseEvent.h>
#include <com_sun_glass_events_KeyEvent.h>

#include <com_sun_glass_ui_Window_Level.h>

#include <X11/extensions/shape.h>
#include <cairo.h>
#include <gdk/gdk.h>

#include <string.h>

#include <algorithm>

#define MOUSE_BACK_BTN 8
#define MOUSE_FORWARD_BTN 9

void destroy_and_delete_ctx(WindowContext *ctx) {
    if (ctx) {
        ctx->process_destroy();

        if (!ctx->get_events_count()) {
            delete ctx;
        }
        // else: ctx will be deleted in EventsCounterHelper after completing
        // an event processing
    }
}

static inline jint gtk_button_number_to_mouse_button(guint button) {
    switch (button) {
        case 1:
            return com_sun_glass_events_MouseEvent_BUTTON_LEFT;
        case 2:
            return com_sun_glass_events_MouseEvent_BUTTON_OTHER;
        case 3:
            return com_sun_glass_events_MouseEvent_BUTTON_RIGHT;
        case MOUSE_BACK_BTN:
            return com_sun_glass_events_MouseEvent_BUTTON_BACK;
        case MOUSE_FORWARD_BTN:
            return com_sun_glass_events_MouseEvent_BUTTON_FORWARD;
        default:
            // Other buttons are not supported by quantum and are not reported by other platforms
            return com_sun_glass_events_MouseEvent_BUTTON_NONE;
    }
}

static void map_cb (GtkWidget* self, gpointer data) {
    g_print("map_cb\n");
    WindowContext *ctx = (WindowContext *)data;
    ctx->process_map();
}

static void process_events_cb(GdkEvent* event, gpointer data) {
    WindowContext *ctx = (WindowContext *)data;

//TODO window disabled
//    if ((window != NULL)
//            && !is_window_enabled_for_event(window, ctx, event->type)) {
//        return;
//    }

//    if (ctx != NULL && ctx->hasIME() && ctx->filterIME(event)) {
//        return;
//    }

//    glass_evloop_call_hooks(event);

    guint type = gdk_event_get_event_type(GDK_EVENT(event));

    g_print("Event: %d\n", type);

    EventsCounterHelper helper(ctx);
    try {
        switch (type) {
//            case GDK_PROPERTY_NOTIFY:
//                ctx->process_property_notify(event);
////                gtk_main_do_event(event);
//                break;
//            case GDK_CONFIGURE:
//                ctx->process_configure(event);
//                gtk_main_do_event(event);
                break;
            case GDK_FOCUS_CHANGE:
                ctx->process_focus(event);
//                gtk_main_do_event(event);
                break;
//TODO use GObject dispose
//            case GDK_DESTROY:
//                destroy_and_delete_ctx(ctx);
////                gtk_main_do_event(event);
//                break;
            case GDK_DELETE:
                ctx->process_delete();
                break;
//            case GDK_EXPOSE:
//            case GDK_DAMAGE:
//                ctx->process_expose(event);
//                break;
//            case GDK_WINDOW_STATE:
//                ctx->process_state(event);
//                gtk_main_do_event(event);
                break;
            case GDK_BUTTON_PRESS:
            case GDK_BUTTON_RELEASE:
                ctx->process_mouse_button(event);
                break;
            case GDK_MOTION_NOTIFY:
                ctx->process_mouse_motion(event);
//                gdk_event_request_motions(event);
                break;
            case GDK_SCROLL:
                ctx->process_mouse_scroll(event);
                break;
            case GDK_ENTER_NOTIFY:
            case GDK_LEAVE_NOTIFY:
                ctx->process_mouse_cross(event);
                break;
            case GDK_KEY_PRESS:
            case GDK_KEY_RELEASE:
                ctx->process_key(event);
                break;
//            case GDK_DROP_START:
//            case GDK_DRAG_ENTER:
//            case GDK_DRAG_LEAVE:
//            case GDK_DRAG_MOTION:
//                process_dnd_target(ctx, &event->dnd);
//                break;
//            case GDK_UNMAP:
//            case GDK_CLIENT_EVENT:
//            case GDK_VISIBILITY_NOTIFY:
//            case GDK_SETTING:
//            case GDK_OWNER_CHANGE:
//                gtk_main_do_event(event);
                break;
            default:
                break;
        }
    } catch (jni_exception&) {
    }
}

static void realize_cb(GtkWidget* self, gpointer data) {
    g_print("Realized\n");
    g_signal_connect(self, "event", G_CALLBACK(process_events_cb), data);
}

static void draw_cb (GtkDrawingArea *drawing_area,
                     cairo_t        *cr,
                     int             width,
                     int             height,
                     gpointer        data) {

    g_print("Draw\n");

    cairo_surface_t *surface = ((WindowContext *)data)->get_cairo_surface();

    if (surface) {
        cairo_set_source_surface(cr, surface, 0, 0);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_paint(cr);
        cairo_surface_destroy(surface);
    }
}

////////////////////////////// WindowContext /////////////////////////////////

WindowContext::WindowContext(jobject _jwindow, WindowContext *_owner, long _screen,
                                   WindowFrameType _frame_type, WindowType type) :
        screen(_screen),
        frame_type(_frame_type),
        window_type(type),
        owner(_owner),
        jview(NULL),
        map_received(false),
        visible_received(false),
        on_top(false),
        is_fullscreen(false),
        is_iconified(false),
        is_maximized(false),
        is_mouse_entered(false),
        can_be_deleted(false),
        events_processing_cnt(0),
        cairo_surface(NULL) {

    jwindow = mainEnv->NewGlobalRef(_jwindow);

    g_print("New WindowContext\n");

    //the actual widget is a drawing area
    gtk_widget = gtk_window_new();

    g_signal_connect(gtk_widget, "realize", G_CALLBACK(realize_cb), this);
    g_signal_connect(gtk_widget, "map", G_CALLBACK(map_cb), this);

    g_print("Phase 1...\n");
    drawing_area = gtk_drawing_area_new();
    gtk_widget_set_halign(drawing_area, GTK_ALIGN_FILL);
    gtk_widget_set_valign(drawing_area, GTK_ALIGN_FILL);
    gtk_window_set_child(GTK_WINDOW(gtk_widget), drawing_area);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), draw_cb, this, NULL);

    g_print("Phase 2...\n");

    if (type == POPUP) {
//        gtk_window_set_skip_taskbar_hint(GTK_WINDOW(gtk_widget), TRUE);
//        gtk_window_set_skip_pager_hint(GTK_WINDOW(gtk_widget), TRUE);
        gtk_window_set_decorated(GTK_WINDOW(gtk_widget), FALSE);

        //TODO: need more properties
        /* Other options:
            gtk_popover_new()
            gdk_surface_new_popup()
        */
    } else {
//        GdkToplevelLayout* layout = gdk_toplevel_layout_new();
        //gdk_toplevel_layout_set_resizable
        //gdk_toplevel_layout_set_maximized
        //gdk_toplevel_layout_set_fullscreen

        if (frame_type != TITLED) {
            gtk_window_set_decorated(GTK_WINDOW(gtk_widget), FALSE);
        }
    }

    // This allows to group Windows (based on WM_CLASS). Use the fully qualified
    // JavaFX Application class as StartupWMClass on the .desktop launcher
//    if (gchar * app_name = get_application_name()) {
//        gtk_window_set_wmclass(GTK_WINDOW(gtk_widget), app_name, app_name);
//        g_free(app_name);
//    }

//    if (owner) {
//        owner->add_child(this);
//        if (on_top_inherited()) {
//            gtk_window_set_keep_above(GTK_WINDOW(gtk_widget), TRUE);
//        }
//    }

    if (type == UTILITY) {
//        gtk_window_set_type_hint(GTK_WINDOW(gtk_window), GDK_WINDOW_TYPE_HINT_UTILITY);
    }

    gtk_window_set_title(GTK_WINDOW(gtk_widget), "");
    g_object_set_data_full(G_OBJECT(gtk_widget), GDK_WINDOW_DATA_CONTEXT, this, NULL);

//    glass_dnd_attach_context(this);

//    gdk_windowManagerFunctions = wmf;
//    if (wmf) {
//        gdk_window_set_functions(gdk_window, wmf);
//    }
}

void WindowContext::paint(void *data, jint width, jint height) {
    if (cairo_surface) {
        cairo_surface_destroy(cairo_surface);
    }

    cairo_surface = cairo_image_surface_create_for_data(
            (unsigned char *) data,
            CAIRO_FORMAT_ARGB32,
            width, height, width * 4);

    gtk_widget_queue_draw(drawing_area);
}

bool WindowContext::isEnabled() {
    if (jwindow) {
        bool result = (JNI_TRUE == mainEnv->CallBooleanMethod(jwindow, jWindowIsEnabled));
        LOG_EXCEPTION(mainEnv)
        return result;
    } else {
        return false;
    }
}

cairo_surface_t *WindowContext::get_cairo_surface() {
    return cairo_surface;
}

GtkWidget *WindowContext::get_gtk_widget() {
    return gtk_widget;
}

GtkWindow *WindowContext::get_gtk_window() {
    return GTK_WINDOW(gtk_widget);
}

WindowGeometry WindowContext::get_geometry() {
    return geometry;
}

jobject WindowContext::get_jwindow() {
    return jwindow;
}

jobject WindowContext::get_jview() {
    return jview;
}

void WindowContext::process_map() {
    map_received = true;
    apply_geometry();
}

void WindowContext::process_focus(GdkEvent *event) {
    gboolean event_in = gdk_focus_event_get_in(event);

    if (xim.enabled && xim.ic) {
        if (event_in) {
            XSetICFocus(xim.ic);
        } else {
            XUnsetICFocus(xim.ic);
        }
    }

    if (jwindow) {
        if (!event_in || isEnabled()) {
            mainEnv->CallVoidMethod(jwindow, jWindowNotifyFocus,
                                    event_in ? com_sun_glass_events_WindowEvent_FOCUS_GAINED
                                              : com_sun_glass_events_WindowEvent_FOCUS_LOST);
            CHECK_JNI_EXCEPTION(mainEnv)
        } else {
            mainEnv->CallVoidMethod(jwindow, jWindowNotifyFocusDisabled);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }
}

//void WindowContext::process_property_notify(GdkPropertyEvent *event) {
//}

void WindowContext::process_configure() {
//    gint x, y, w, h, gtk_w, gtk_h;
//
////    gtk_window_get_position(GTK_WINDOW(gtk_window), &x, &y);
//    gdk_surface_get_height(gdk_surface, &h);
//    gdk_surface_get_width(gdk_surface, &x);
//
////    gtk_window_get_size(GTK_WINDOW(gtk_window), &gtk_w, &gtk_h);
//
//    w = gtk_w;
//    h = gtk_h;
//
////    gboolean pos_changed = geometry.current_x != x || geometry.current_y != y;
//    gboolean size_changed = geometry.current_w != w || geometry.current_h != h
//                            || geometry.current_cw != gtk_w || geometry.current_ch != gtk_h;
//
//    geometry.current_x = x;
//    geometry.current_y = y;
//    geometry.current_w = w;
//    geometry.current_h = h;
//    geometry.current_cw = gtk_w;
//    geometry.current_ch = gtk_h;
//
//    if (!is_fullscreen && !is_maximized) {
//        geometry.last_cw = gtk_w;
//        geometry.last_ch = gtk_h;
//    }
//
//    size_position_notify(size_changed, pos_changed);
}

void WindowContext::process_destroy() {
    if (owner) {
        owner->remove_child(this);
    }

    std::set<WindowContext *>::iterator it;
    for (it = children.begin(); it != children.end(); ++it) {
        // FIX JDK-8226537: this method calls set_owner(NULL) which prevents
        // WindowContext::process_destroy() to call remove_child() (because children
        // is being iterated here) but also prevents gtk_window_set_transient_for from
        // being called - this causes the crash on gnome.
        gtk_window_set_transient_for((*it)->get_gtk_window(), NULL);
        (*it)->set_owner(NULL);
        destroy_and_delete_ctx(*it);
    }
    children.clear();

    if (jwindow) {
        mainEnv->CallVoidMethod(jwindow, jWindowNotifyDestroy);
        EXCEPTION_OCCURED(mainEnv);
    }

    if (jview) {
        mainEnv->DeleteGlobalRef(jview);
        jview = NULL;
    }

    if (jwindow) {
        mainEnv->DeleteGlobalRef(jwindow);
        jwindow = NULL;
    }

    can_be_deleted = true;
}

void WindowContext::process_delete() {
    if (jwindow && isEnabled()) {
//TODO
//        gtk_widget_hide_on_delete(gtk_widget);
        mainEnv->CallVoidMethod(jwindow, jWindowNotifyClose);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

//void WindowContext::process_expose(GdkEventExpose *event) {
//    if (jview && is_visible()) {
//        mainEnv->CallVoidMethod(jview, jViewNotifyRepaint, event->area.x, event->area.y,
//                                event->area.width, event->area.height);
//        CHECK_JNI_EXCEPTION(mainEnv)
//    }
//}

void WindowContext::process_mouse_button(GdkEvent *event) {
    // there are other events like GDK_2BUTTON_PRESS
    GdkEventType type = gdk_event_get_event_type(event);

    if (type != GDK_BUTTON_PRESS && type != GDK_BUTTON_RELEASE) {
        return;
    }

    bool press = type == GDK_BUTTON_PRESS;

    guint state = gdk_event_get_modifier_state(event);
    guint mask = 0;
    guint button = gdk_button_event_get_button(event);

    // We need to add/remove current mouse button from the modifier flags
    // as X lib state represents the state just prior to the event and
    // glass needs the state just after the event
    switch (button) {
        case 1:
            mask = GDK_BUTTON1_MASK;
            break;
        case 2:
            mask = GDK_BUTTON2_MASK;
            break;
        case 3:
            mask = GDK_BUTTON3_MASK;
            break;
        case MOUSE_BACK_BTN:
            mask = GDK_BUTTON4_MASK;
            break;
        case MOUSE_FORWARD_BTN:
            mask = GDK_BUTTON5_MASK;
            break;
    }

    if (press) {
        state |= mask;
    } else {
        state &= ~mask;
    }

    bool is_popup_trigger = (button == 3);

    //FIXME
    //jint button = gtk_button_number_to_mouse_button(button);

    double x, y, x_root, y_root;
    gdk_event_get_position(event, &x, &y);

    //TODO
    x_root = 0;
    y_root = 0;

    if (jview && button != com_sun_glass_events_MouseEvent_BUTTON_NONE) {
        mainEnv->CallVoidMethod(jview, jViewNotifyMouse,
                                press ? com_sun_glass_events_MouseEvent_DOWN : com_sun_glass_events_MouseEvent_UP,
                                button,
                                (jint) x, (jint) y,
                                (jint) x_root, (jint) y_root,
                                gdk_modifier_mask_to_glass(state),
                                (is_popup_trigger) ? JNI_TRUE : JNI_FALSE,
                                JNI_FALSE);
        CHECK_JNI_EXCEPTION(mainEnv)

        if (jview && is_popup_trigger) {
            mainEnv->CallVoidMethod(jview, jViewNotifyMenu,
                                    (jint) x, (jint) y,
                                    (jint) x_root, y_root,
                                    JNI_FALSE);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }
}

void WindowContext::process_mouse_motion(GdkEvent *event) {
    guint state = gdk_event_get_modifier_state(event);

    jint glass_modifier = gdk_modifier_mask_to_glass(state);

    jint isDrag = glass_modifier & (
            com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_PRIMARY |
            com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_MIDDLE |
            com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_SECONDARY |
            com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_BACK |
            com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_FORWARD);
    jint button = com_sun_glass_events_MouseEvent_BUTTON_NONE;

    if (glass_modifier & com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_PRIMARY) {
        button = com_sun_glass_events_MouseEvent_BUTTON_LEFT;
    } else if (glass_modifier & com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_MIDDLE) {
        button = com_sun_glass_events_MouseEvent_BUTTON_OTHER;
    } else if (glass_modifier & com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_SECONDARY) {
        button = com_sun_glass_events_MouseEvent_BUTTON_RIGHT;
    } else if (glass_modifier & com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_BACK) {
        button = com_sun_glass_events_MouseEvent_BUTTON_BACK;
    } else if (glass_modifier & com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_FORWARD) {
        button = com_sun_glass_events_MouseEvent_BUTTON_FORWARD;
    }

    if (jview) {
        double x, y, x_root, y_root;
        gdk_event_get_position(event, &x, &y);

        //TODO
        x_root = 0;
        y_root = 0;

        mainEnv->CallVoidMethod(jview, jViewNotifyMouse,
                                isDrag ? com_sun_glass_events_MouseEvent_DRAG : com_sun_glass_events_MouseEvent_MOVE,
                                button,
                                (jint) x, (jint) y,
                                (jint) x_root, (jint) y_root,
                                glass_modifier,
                                JNI_FALSE,
                                JNI_FALSE);
        CHECK_JNI_EXCEPTION(mainEnv)
    }

//    gdk_event_request_motions(event);
}

void WindowContext::process_mouse_scroll(GdkEvent *event) {
    jdouble dx = 0, dy = 0;

    // converting direction to change in pixels
    GdkScrollDirection direction = gdk_scroll_event_get_direction(event);

    switch (direction) {
        case GDK_SCROLL_UP:
            dy = 1;
            break;
        case GDK_SCROLL_DOWN:
            dy = -1;
            break;
        case GDK_SCROLL_LEFT:
            dx = 1;
            break;
        case GDK_SCROLL_RIGHT:
            dx = -1;
            break;
        default:
            break;
    }

    guint state = gdk_event_get_modifier_state(event);

    if (state & GDK_SHIFT_MASK) {
        jdouble t = dy;
        dy = dx;
        dx = t;
    }

    if (jview) {
        double x, y, x_root, y_root;
        gdk_event_get_position(event, &x, &y);

        //TODO
        x_root = 0;
        y_root = 0;

        mainEnv->CallVoidMethod(jview, jViewNotifyScroll,
                                (jint) x, (jint) y,
                                (jint) x_root, (jint) y_root,
                                dx, dy,
                                gdk_modifier_mask_to_glass(state),
                                (jint) 0, (jint) 0,
                                (jint) 0, (jint) 0,
                                (jdouble) 40.0, (jdouble) 40.0);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

void WindowContext::process_mouse_cross(GdkEvent *event) {
    GdkEventType type = gdk_event_get_event_type(event);

    bool enter = type == GDK_ENTER_NOTIFY;

    if (jview) {
        guint state = gdk_event_get_modifier_state(event);

        if (enter) { // workaround for RT-21590
            state &= ~MOUSE_BUTTONS_MASK;
        }

        double x, y, x_root, y_root;
        gdk_event_get_position(event, &x, &y);

        //TODO: calculate x_root, y_root
        //The coordinate of the pointer relative to the root of the screen.
        y_root = 0;
        x_root = 0;

        if (enter != is_mouse_entered) {
            is_mouse_entered = enter;
            mainEnv->CallVoidMethod(jview, jViewNotifyMouse,
                                    enter ? com_sun_glass_events_MouseEvent_ENTER
                                          : com_sun_glass_events_MouseEvent_EXIT,
                                    com_sun_glass_events_MouseEvent_BUTTON_NONE,
                                    (jint) x, (jint) y,
                                    (jint) x_root, (jint) y_root,
                                    gdk_modifier_mask_to_glass(state),
                                    JNI_FALSE,
                                    JNI_FALSE);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }
}

void WindowContext::process_key(GdkEvent *event) {
    bool press = gdk_event_get_event_type(event) == GDK_KEY_PRESS;
    guint state = gdk_event_get_modifier_state(event);

    jint glassKey = get_glass_key(event);
    jint glassModifier = gdk_modifier_mask_to_glass(state);
    if (press) {
        glassModifier |= glass_key_to_modifier(glassKey);
    } else {
        glassModifier &= ~glass_key_to_modifier(glassKey);
    }
    jcharArray jChars = NULL;

    jchar key = gdk_keyval_to_unicode(gdk_key_event_get_keyval(GDK_EVENT(event)));

    if (key >= 'a' && key <= 'z' && (state & GDK_CONTROL_MASK)) {
        key = key - 'a' + 1; // map 'a' to ctrl-a, and so on.
    }

    if (key > 0) {
        jChars = mainEnv->NewCharArray(1);
        if (jChars) {
            mainEnv->SetCharArrayRegion(jChars, 0, 1, &key);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    } else {
        jChars = mainEnv->NewCharArray(0);
    }
    if (jview) {
        if (press) {
            mainEnv->CallVoidMethod(jview, jViewNotifyKey,
                                    com_sun_glass_events_KeyEvent_PRESS,
                                    glassKey,
                                    jChars,
                                    glassModifier);
            CHECK_JNI_EXCEPTION(mainEnv)

            if (jview && key > 0) { // TYPED events should only be sent for printable characters.
                mainEnv->CallVoidMethod(jview, jViewNotifyKey,
                                        com_sun_glass_events_KeyEvent_TYPED,
                                        com_sun_glass_events_KeyEvent_VK_UNDEFINED,
                                        jChars,
                                        glassModifier);
                CHECK_JNI_EXCEPTION(mainEnv)
            }
        } else {
            mainEnv->CallVoidMethod(jview, jViewNotifyKey,
                                    com_sun_glass_events_KeyEvent_RELEASE,
                                    glassKey,
                                    jChars,
                                    glassModifier);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }
}

//TODO
//void WindowContext::process_state(GdkEventWindowState *event) {
//    if (event->changed_mask &
//        (GDK_WINDOW_STATE_ICONIFIED | GDK_WINDOW_STATE_MAXIMIZED)) {
//
//        if (event->changed_mask & GDK_WINDOW_STATE_ICONIFIED) {
//            is_iconified = event->new_window_state & GDK_WINDOW_STATE_ICONIFIED;
//        }
//        if (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) {
//            is_maximized = event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED;
//        }
//
//        jint stateChangeEvent;
//
//        if (is_iconified) {
//            stateChangeEvent = com_sun_glass_events_WindowEvent_MINIMIZE;
//        } else if (is_maximized) {
//            stateChangeEvent = com_sun_glass_events_WindowEvent_MAXIMIZE;
//        } else {
//            stateChangeEvent = com_sun_glass_events_WindowEvent_RESTORE;
//            if ((gdk_windowManagerFunctions & GDK_FUNC_MINIMIZE) == 0) {
//                // in this case - the window manager will not support the programatic
//                // request to iconify - so we need to restore it now.
//                gdk_window_set_functions(gdk_window, gdk_windowManagerFunctions);
//            }
//        }
//
//        notify_state(stateChangeEvent);
//    } else if (event->changed_mask & GDK_WINDOW_STATE_ABOVE) {
//        notify_on_top(event->new_window_state & GDK_WINDOW_STATE_ABOVE);
//    }
//}

void WindowContext::process_screen_changed() {
//FIXME
//    GdkMonitor *monitor = gdk_display_get_monitor_at_surface(gdk_display_get_default(), gdk_surface);

//    glong to_screen = getScreenPtrForLocation(geometry.current_x, geometry.current_y);
//
//    if (to_screen != -1) {
//        if (to_screen != screen) {
//            if (jwindow) {
//                //notify screen changed
//                jobject jScreen = createJavaScreen(mainEnv, to_screen);
//                mainEnv->CallVoidMethod(jwindow, jWindowNotifyMoveToAnotherScreen, jScreen);
//                CHECK_JNI_EXCEPTION(mainEnv)
//            }
//            screen = to_screen;
//        }
//    }
}

void WindowContext::notify_on_top(bool top) {
    // Do not report effective (i.e. native) values to the FX, only if the user sets it manually
    if (top != effective_on_top() && jwindow) {
        if (on_top_inherited() && !top) {
            // Disallow user's "on top" handling on windows that inherited the property
            //TODO
//            gtk_window_set_keep_above(GTK_WINDOW(gtk_window), TRUE);
        } else {
            on_top = top;
            update_ontop_tree(top);
            mainEnv->CallVoidMethod(jwindow,
                                    jWindowNotifyLevelChanged,
                                    top ? com_sun_glass_ui_Window_Level_FLOATING
                                        : com_sun_glass_ui_Window_Level_NORMAL);
            CHECK_JNI_EXCEPTION(mainEnv);
        }
    }
}

void WindowContext::notify_repaint() {
    if (jview) {
        //TODO: probably get drawing area size
        GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(gtk_widget));

        int w, h;
        w = gdk_surface_get_width(surface);
        h = gdk_surface_get_height(surface);

        mainEnv->CallVoidMethod(jview,
                                jViewNotifyRepaint,
                                0, 0, w, h);
        CHECK_JNI_EXCEPTION(mainEnv);
    }
}

void WindowContext::notify_state(jint glass_state) {
    if (glass_state == com_sun_glass_events_WindowEvent_RESTORE) {
        if (is_maximized) {
            glass_state = com_sun_glass_events_WindowEvent_MAXIMIZE;
        }

        notify_repaint();
    }

    if (jwindow) {
        mainEnv->CallVoidMethod(jwindow,
                                jGtkWindowNotifyStateChanged,
                                glass_state);
        CHECK_JNI_EXCEPTION(mainEnv);
    }
}

bool WindowContext::set_view(jobject view) {
    if (jview) {
        mainEnv->CallVoidMethod(jview, jViewNotifyMouse,
                                com_sun_glass_events_MouseEvent_EXIT,
                                com_sun_glass_events_MouseEvent_BUTTON_NONE,
                                0, 0,
                                0, 0,
                                0,
                                JNI_FALSE,
                                JNI_FALSE);
        mainEnv->DeleteGlobalRef(jview);
    }

    if (view) {
        jview = mainEnv->NewGlobalRef(view);
    } else {
        jview = NULL;
    }
    return TRUE;
}

void WindowContext::set_visible(bool visible) {
    if (visible) {
        gtk_widget_show(gtk_widget);
    } else {
        gtk_widget_hide(gtk_widget);
        if (jview && is_mouse_entered) {
            is_mouse_entered = false;
            mainEnv->CallVoidMethod(jview, jViewNotifyMouse,
                                    com_sun_glass_events_MouseEvent_EXIT,
                                    com_sun_glass_events_MouseEvent_BUTTON_NONE,
                                    0, 0,
                                    0, 0,
                                    0,
                                    JNI_FALSE,
                                    JNI_FALSE);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }

    if (visible) {
        visible_received = TRUE;
    }

    //JDK-8220272 - fire event first because GDK_FOCUS_CHANGE is not always in order
    if (visible && jwindow && isEnabled()) {
        mainEnv->CallVoidMethod(jwindow, jWindowNotifyFocus, com_sun_glass_events_WindowEvent_FOCUS_GAINED);
        CHECK_JNI_EXCEPTION(mainEnv);
    }
}

void WindowContext::set_cursor(GdkCursor *cursor) {
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(gtk_widget));
    gdk_surface_set_cursor(surface, cursor);
}

void WindowContext::set_level(int level) {
    if (level == com_sun_glass_ui_Window_Level_NORMAL) {
        on_top = false;
    } else if (level == com_sun_glass_ui_Window_Level_FLOATING
               || level == com_sun_glass_ui_Window_Level_TOPMOST) {
        on_top = true;
    }
    // We need to emulate always on top behaviour on child windows

    if (!on_top_inherited()) {
        update_ontop_tree(on_top);
    }
}

void WindowContext::set_background(float r, float g, float b) {
    bg_color.red = r;
    bg_color.green = g;
    bg_color.blue = b;
    bg_color.is_set = true;
    notify_repaint();
}

void WindowContext::set_minimized(bool minimize) {
    is_iconified = minimize;
    if (minimize) {
//        if (frame_type == TRANSPARENT) {
//            // https://bugs.launchpad.net/ubuntu/+source/unity/+bug/1245571
//            glass_window_reset_input_shape_mask(gtk_widget_get_window(gtk_widget));
//        }

//        if ((gdk_windowManagerFunctions & GDK_FUNC_MINIMIZE) == 0) {
//            // in this case - the window manager will not support the programatic
//            // request to iconify - so we need to disable this until we are restored.
//            GdkWMFunction wmf = (GdkWMFunction)(gdk_windowManagerFunctions | GDK_FUNC_MINIMIZE);
//            gdk_window_set_functions(gdk_window, wmf);
//        }

//        gdk_toplevel_minimize(GDK_TOPLEVEL(gtk_window));
    } else {
//        gtk_window_present(GTK_WINDOW(gtk_window));
    }
}

void WindowContext::set_maximized(bool maximize) {
    is_maximized = maximize;

    if (maximize) {
        // enable the functionality
//        GdkWMFunction wmf = (GdkWMFunction)(gdk_windowManagerFunctions | GDK_FUNC_MAXIMIZE);
//        gdk_window_set_functions(gdk_window, wmf);

//        gtk_window_maximize(GTK_WINDOW(gtk_window));
    } else {
//        gtk_window_unmaximize(GTK_WINDOW(gtk_window));
    }
}

void WindowContext::set_bounds(int x, int y, bool xSet, bool ySet, int w, int h, int cw, int ch) {
    int newW, newH;

    geometry.current_w = w;
    geometry.current_h = h;
    geometry.current_cw = cw;
    geometry.current_ch = ch;

    gboolean size_changed = FALSE;
    gboolean pos_changed = FALSE;

    if (cw > -1 || w > -1) {
        size_changed = TRUE;
        apply_geometry();

        if (cw > -1) {
            gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(drawing_area), w);
            gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(drawing_area), h);
        }

        if (w > -1) {
            gtk_window_set_default_size(GTK_WINDOW(gtk_widget), w, h);
        }
    }

    if (xSet || ySet) {
        int newX = (xSet) ? x : geometry.current_x;
        int newY = (ySet) ? y : geometry.current_y;

        if (newX != geometry.current_x || newY != geometry.current_y) {
            pos_changed = TRUE;
            geometry.current_x = newX;
            geometry.current_y = newY;

// TODO: use X11
//            gtk_window_move(GTK_WINDOW(gtk_window), newX, newY);
        }
    }

    size_position_notify(size_changed, pos_changed);
}

void WindowContext::set_resizable(bool res) {
    if (res != geometry.resizable) {
        geometry.resizable = res;
        apply_geometry();
    }
}

void WindowContext::set_focusable(bool focusable) {
//    gtk_window_set_accept_focus(GTK_WINDOW(gtk_window)), focusable ? TRUE : FALSE);
    gtk_widget_set_can_focus(gtk_widget, focusable ? TRUE : FALSE);
}

void WindowContext::set_title(const char *title) {
    gtk_window_set_title(GTK_WINDOW(gtk_widget), title);
}

void WindowContext::set_alpha(double alpha) {
    gtk_widget_set_opacity(gtk_widget, (gdouble)alpha);
}

void WindowContext::set_enabled(bool enabled) {
    if (enabled != geometry.enabled) {
        gtk_widget_set_sensitive(gtk_widget, enabled);
        geometry.enabled = enabled;
        apply_geometry();
    }
}

void WindowContext::set_minimum_size(int w, int h) {
    gboolean changed = geometry.minw != w || geometry.minh != h;

    if (!changed) {
        return;
    }

    geometry.minw = w;
    geometry.minh = h;

    apply_geometry();
}

void WindowContext::set_maximum_size(int w, int h) {
    gboolean changed = geometry.maxw != w || geometry.maxh != h;

    if (!changed) {
        return;
    }

    geometry.maxw = w;
    geometry.maxh = h;

    apply_geometry();
}

void WindowContext::set_icon(GdkPixbuf *pixbuf) {
//FIXME: removed
//    gtk_window_set_icon(GTK_WINDOW(gtk_window), pixbuf);
}

//not used
void WindowContext::set_modal(bool modal, WindowContext *parent) {
//    if (modal) {
//        if (parent) {
//            gtk_window_set_transient_for(GTK_WINDOW(gtk_window), parent->get_gtk_window());
//        }
//    }
//    gtk_window_set_modal(GTK_WINDOW(gtk_window), modal ? TRUE : FALSE);
}

void WindowContext::set_gravity(float x, float y) {
}

void WindowContext::set_owner(WindowContext *owner_ctx) {
    owner = owner_ctx;
}

void WindowContext::add_child(WindowContext *child) {
    children.insert(child);
    gtk_window_set_transient_for(child->get_gtk_window(), this->get_gtk_window());
}

void WindowContext::remove_child(WindowContext *child) {
    children.erase(child);
    gtk_window_set_transient_for(child->get_gtk_window(), NULL);
}

void WindowContext::show_or_hide_children(bool show) {
    std::set<WindowContext *>::iterator it;
    for (it = children.begin(); it != children.end(); ++it) {
        (*it)->set_minimized(!show);
        (*it)->show_or_hide_children(show);
    }
}

bool WindowContext::is_visible() {
    return gtk_widget_get_visible(gtk_widget);
}

bool WindowContext::is_dead() {
    return can_be_deleted;
}

void WindowContext::restack(bool restack) {
//FIXME: removed
//    gdk_window_restack(gdk_window, NULL, restack ? TRUE : FALSE);
}

void WindowContext::request_focus() {
    //JDK-8212060: Window show and then move glitch.
    //The WindowContext::set_visible will take care of showing the window.
    //The below code will only handle later request_focus.
    if (is_visible()) {
        //gtk_window_present(GTK_WINDOW(gtk_window));
    }
}

void WindowContext::enter_fullscreen() {
    is_fullscreen = TRUE;

    gtk_window_fullscreen(GTK_WINDOW(gtk_widget));
}

void WindowContext::exit_fullscreen() {
    is_fullscreen = FALSE;
    gtk_window_unfullscreen(GTK_WINDOW(gtk_widget));
}

// Applied to a temporary full screen window to prevent sending events to Java
void WindowContext::detach_from_java() {
    if (jview) {
        mainEnv->DeleteGlobalRef(jview);
        jview = NULL;
    }
    if (jwindow) {
        mainEnv->DeleteGlobalRef(jwindow);
        jwindow = NULL;
    }
}

void WindowContext::increment_events_counter() {
    ++events_processing_cnt;
}

void WindowContext::decrement_events_counter() {
    --events_processing_cnt;
}

size_t WindowContext::get_events_count() {
    return events_processing_cnt;
}

///////////////////////// PROTECTED

void WindowContext::applyShapeMask(void *data, uint width, uint height) {
    if (frame_type != TRANSPARENT) {
        return;
    }
}

///////////////////////// PRIVATE

void WindowContext::apply_geometry() {
//FIXME: Removed
//    if (!map_received) {
//        return;
//    }
//
//    GdkGeometry gdk_geometry;
//    gdk_geometry.win_gravity = GDK_GRAVITY_NORTH_WEST;
//
//    //FIXME: probably wrong
//    if ((!geometry.resizable || !geometry.enabled) && !(is_maximized || is_fullscreen)) {
//        // not resizeable
//        int w = geometry.current_cw > 0
//                ? geometry.current_cw
//                : geometry.current_w;
//
//        int h = geometry.current_ch > 0
//                ? geometry.current_ch
//                : geometry.current_h;
//
//        gdk_geometry.min_width = gdk_geometry.max_width = w;
//        gdk_geometry.min_height = gdk_geometry.max_height = h;
//    } else {
//        //min/max width/height always whole window size (with decors)
//        gdk_geometry.min_width = geometry.minw > 0 ? geometry.minw  : 1;
//        gdk_geometry.min_height = geometry.minh > 0 ? geometry.minh : 1;
//
//        gdk_geometry.max_width = geometry.maxw > 0 ? geometry.maxw : G_MAXINT;
//        gdk_geometry.max_height = geometry.maxh > 0 ? geometry.maxh : G_MAXINT;
//    }
//    gtk_window_set_geometry_hints(GTK_WINDOW(gtk_window), NULL, &gdk_geometry,
//                                  (GdkWindowHints)(GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE | GDK_HINT_WIN_GRAVITY));
}

void WindowContext::size_position_notify(bool size_changed, bool pos_changed) {

    if (jview) {
        if (size_changed) {
            mainEnv->CallVoidMethod(jview, jViewNotifyResize, geometry.current_cw, geometry.current_ch);
            CHECK_JNI_EXCEPTION(mainEnv);
        }

        if (pos_changed) {
            mainEnv->CallVoidMethod(jview, jViewNotifyView, com_sun_glass_events_ViewEvent_MOVE);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }

    if (jwindow) {
        if (size_changed || is_maximized) {
            mainEnv->CallVoidMethod(jwindow, jWindowNotifyResize,
                                    (is_maximized)
                                    ? com_sun_glass_events_WindowEvent_MAXIMIZE
                                    : com_sun_glass_events_WindowEvent_RESIZE,
                                    geometry.current_w, geometry.current_h);
            CHECK_JNI_EXCEPTION(mainEnv)
        }

        if (pos_changed) {
            mainEnv->CallVoidMethod(jwindow, jWindowNotifyMove, geometry.current_x, geometry.current_y);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }
}

void WindowContext::update_ontop_tree(bool on_top) {
    bool effective_on_top = on_top || this->on_top;
//FIXME: removed
//    gtk_window_set_keep_above(GTK_WINDOW(gtk_window), effective_on_top ? TRUE : FALSE);
    for (std::set<WindowContext *>::iterator it = children.begin(); it != children.end(); ++it) {
        (*it)->update_ontop_tree(effective_on_top);
    }
}

bool WindowContext::on_top_inherited() {
    WindowContext *o = owner;
    while (o) {
        WindowContext *topO = dynamic_cast<WindowContext *>(o);
        if (!topO) break;
        if (topO->on_top) {
            return true;
        }
        o = topO->owner;
    }
    return false;
}

bool WindowContext::effective_on_top() {
    if (owner) {
        WindowContext *topO = dynamic_cast<WindowContext *>(owner);
        return (topO && topO->effective_on_top()) || on_top;
    }
    return on_top;
}

WindowContext::~WindowContext() {
    if (xim.ic) {
        XDestroyIC(xim.ic);
        xim.ic = NULL;
    }
    if (xim.im) {
        XCloseIM(xim.im);
        xim.im = NULL;
    }

    gtk_window_destroy(GTK_WINDOW(gtk_widget));
}
