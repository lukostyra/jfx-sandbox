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
#include "glass_window.h"
#include "glass_general.h"
#include "glass_key.h"
#include "glass_screen.h"
#include "glass_dnd.h"

#include <com_sun_glass_events_WindowEvent.h>
#include <com_sun_glass_events_ViewEvent.h>
#include <com_sun_glass_events_MouseEvent.h>
#include <com_sun_glass_events_KeyEvent.h>

#include <com_sun_glass_ui_Window_Level.h>

#include <string.h>
#include <algorithm>

WindowContext * WindowContextBase::sm_grab_window = NULL;
WindowContext * WindowContextBase::sm_mouse_drag_window = NULL;

XID WindowContextBase::get_window_xid() {
    return xwindow;
}

jobject WindowContextBase::get_jview() {
    return jview;
}

jobject WindowContextBase::get_jwindow() {
    return jwindow;
}

bool WindowContextBase::isEnabled() {
    if (jwindow) {
        bool result = (JNI_TRUE == mainEnv->CallBooleanMethod(jwindow, jWindowIsEnabled));
        LOG_EXCEPTION(mainEnv)
        return result;
    } else {
        return false;
    }
}

void WindowContextBase::notify_state(jint glass_state) {
    if (glass_state == com_sun_glass_events_WindowEvent_RESTORE) {
        if (is_maximized) {
            glass_state = com_sun_glass_events_WindowEvent_MAXIMIZE;
        }

//        int w, h;
//        glass_gdk_window_get_size(gdk_window, &w, &h);
//        if (jview) {
//            mainEnv->CallVoidMethod(jview,
//                    jViewNotifyRepaint,
//                    0, 0, w, h);
//            CHECK_JNI_EXCEPTION(mainEnv);
//        }
    }

    if (jwindow) {
       mainEnv->CallVoidMethod(jwindow,
               jGtkWindowNotifyStateChanged,
               glass_state);
       CHECK_JNI_EXCEPTION(mainEnv);
    }
}

void WindowContextBase::process_state(GdkEventWindowState* event) {
    if (event->changed_mask &
            (GDK_WINDOW_STATE_ICONIFIED | GDK_WINDOW_STATE_MAXIMIZED)) {

        if (event->changed_mask & GDK_WINDOW_STATE_ICONIFIED) {
            is_iconified = event->new_window_state & GDK_WINDOW_STATE_ICONIFIED;
        }
        if (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) {
            is_maximized = event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED;
        }

        jint stateChangeEvent;

        if (is_iconified) {
            stateChangeEvent = com_sun_glass_events_WindowEvent_MINIMIZE;
        } else if (is_maximized) {
            stateChangeEvent = com_sun_glass_events_WindowEvent_MAXIMIZE;
        } else {
            stateChangeEvent = com_sun_glass_events_WindowEvent_RESTORE;
            if ((gdk_windowManagerFunctions & GDK_FUNC_MINIMIZE) == 0) {
                // in this case - the window manager will not support the programatic
                // request to iconify - so we need to restore it now.
//                gdk_window_set_functions(gdk_window, gdk_windowManagerFunctions);
            }
        }

        notify_state(stateChangeEvent);
    } else if (event->changed_mask & GDK_WINDOW_STATE_ABOVE) {
        notify_on_top( event->new_window_state & GDK_WINDOW_STATE_ABOVE);
    }
}

void WindowContextBase::process_visibility(XVisibilityEvent* event) {
    visibility_state = event->state;
    g_print("Visibility %d\n", visibility_state);
}

void WindowContextBase::process_focus(XFocusChangeEvent* event) {
    bool in = event->type == FocusIn;

    if (!in && WindowContextBase::sm_mouse_drag_window == this) {
        ungrab_mouse_drag_focus();
    }
    if (!in && WindowContextBase::sm_grab_window == this) {
        ungrab_focus();
    }

    if (xim.enabled && xim.ic) {
        if (in) {
            XSetICFocus(xim.ic);
        } else {
            XUnsetICFocus(xim.ic);
        }
    }

    if (jwindow) {
        if (!in || isEnabled()) {
            g_print("jWindowNotifyFocus\n");
            mainEnv->CallVoidMethod(jwindow, jWindowNotifyFocus,
                        in ? com_sun_glass_events_WindowEvent_FOCUS_GAINED : com_sun_glass_events_WindowEvent_FOCUS_LOST);
            CHECK_JNI_EXCEPTION(mainEnv)
        } else {
            mainEnv->CallVoidMethod(jwindow, jWindowNotifyFocusDisabled);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }
}

void WindowContextBase::increment_events_counter() {
    ++events_processing_cnt;
}

void WindowContextBase::decrement_events_counter() {
    --events_processing_cnt;
}

size_t WindowContextBase::get_events_count() {
    return events_processing_cnt;
}

bool WindowContextBase::is_dead() {
    return can_be_deleted;
}

void destroy_and_delete_ctx(WindowContext* ctx) {
    if (ctx) {
        ctx->process_destroy();

        if (!ctx->get_events_count()) {
            delete ctx;
        }
        // else: ctx will be deleted in EventsCounterHelper after completing
        // an event processing
    }
}

void WindowContextBase::process_destroy() {
    if (WindowContextBase::sm_mouse_drag_window == this) {
        ungrab_mouse_drag_focus();
    }

    if (WindowContextBase::sm_grab_window == this) {
        ungrab_focus();
    }

    std::set<WindowContextTop*>::iterator it;
    for (it = children.begin(); it != children.end(); ++it) {
        // FIX JDK-8226537: this method calls set_owner(NULL) which prevents
        // WindowContextTop::process_destroy() to call remove_child() (because children
        // is being iterated here) but also prevents gtk_window_set_transient_for from
        // being called - this causes the crash on gnome.
//        gtk_window_set_transient_for((*it)->get_gtk_window(), NULL);
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

void WindowContextBase::process_delete() {
    if (jwindow && isEnabled()) {
        mainEnv->CallVoidMethod(jwindow, jWindowNotifyClose);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

void WindowContextBase::process_expose(XExposeEvent* event) {
    if (jview) {
        g_print("jViewNotifyRepaint %d, %d, %d, %d\n", event->x, event->y, event->width, event->height);
        mainEnv->CallVoidMethod(jview, jViewNotifyRepaint, event->x, event->y, event->width, event->height);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

void WindowContextBase::process_damage(XDamageNotifyEvent* event) {
    if (jview) {
        g_print("DAMAGE: jViewNotifyRepaint %d, %d, %d, %d\n", event->area.x, event->area.y, event->area.width, event->area.height);
        mainEnv->CallVoidMethod(jview, jViewNotifyRepaint, event->area.x, event->area.y, event->area.width, event->area.height);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

static inline jint xlib_button_number_to_mouse_button(guint button) {
    switch (button) {
        case 1:
            return com_sun_glass_events_MouseEvent_BUTTON_LEFT;
        case 2:
            return com_sun_glass_events_MouseEvent_BUTTON_OTHER;
        case 3:
            return com_sun_glass_events_MouseEvent_BUTTON_RIGHT;
        case 4:
            return com_sun_glass_events_MouseEvent_BUTTON_FORWARD;
        case 5:
            return com_sun_glass_events_MouseEvent_BUTTON_BACK;
        default:
            // Other buttons are not supported by quantum and are not reported by other platforms
            return com_sun_glass_events_MouseEvent_BUTTON_NONE;
    }
}

void WindowContextBase::process_mouse_button(XButtonEvent *event) {
    bool press = event->type == ButtonPress;
    jint state = xlib_modifier_mask_to_glass(event->state);

    int mask = 0;
    switch (event->button) {
        case 1:
            mask = Button1Mask;
            break;
        case 2:
            mask = Button2Mask;
            break;
        case 3:
            mask = Button3Mask;
            break;
        case 4:
            mask = Button4Mask;
            break;
        case 5:
            mask = Button5Mask;
            break;
    }

    if (press) {
        state |= mask;
    } else {
        state &= ~mask;
    }

    g_print("Mouse button %d\n", event->button);
    //scroll
    if (event->button == 4 || event->button == 5) {
        jdouble dx = 0;
        jdouble dy = (event->button == 4) ? 1 : -1;

        if (event->state & ShiftMask) {
            jdouble t = dy;
            dy = dx;
            dx = t;
        }

        if (jview) {
            mainEnv->CallVoidMethod(jview, jViewNotifyScroll,
                    (jint) event->x, (jint) event->y,
                    (jint) event->x_root, (jint) event->y_root,
                    dx, dy, state,
                    (jint) 0, (jint) 0,
                    (jint) 0, (jint) 0,
                    (jdouble) 40.0, (jdouble) 40.0);
            CHECK_JNI_EXCEPTION(mainEnv)
        }

        return;
    }

    jint button = xlib_button_number_to_mouse_button(event->button);

    if (jview && button != com_sun_glass_events_MouseEvent_BUTTON_NONE) {
        g_print("jViewNotifyMouse press=%d, btn=%d, x,y=%d,%d\n", press, button, event->x, event->y);
        mainEnv->CallVoidMethod(jview, jViewNotifyMouse,
                press ? com_sun_glass_events_MouseEvent_DOWN : com_sun_glass_events_MouseEvent_UP,
                button,
                (jint) event->x, (jint) event->y,
                (jint) event->x_root, (jint) event->y_root,
                state,
                (event->button == 3 && press) ? JNI_TRUE : JNI_FALSE,
                JNI_FALSE);
        CHECK_JNI_EXCEPTION(mainEnv)

        if (jview && event->button == 3 && press) {
            mainEnv->CallVoidMethod(jview, jViewNotifyMenu,
                    (jint)event->x, (jint)event->y,
                    (jint)event->x_root, (jint)event->y_root,
                    JNI_FALSE);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }
}


void WindowContextBase::process_mouse_motion(XMotionEvent* event) {
    jint glass_modifier = xlib_modifier_mask_to_glass(event->state);

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

//    g_print("Mouse motion %d, %d, %d, %d\n", event->x_root, event->y_root, event->x, event->y);
    if (jview) {
        mainEnv->CallVoidMethod(jview, jViewNotifyMouse,
                isDrag ? com_sun_glass_events_MouseEvent_DRAG : com_sun_glass_events_MouseEvent_MOVE,
                button,
                (jint) event->x, (jint) event->y,
                (jint) event->x_root, (jint) event->y_root,
                glass_modifier,
                JNI_FALSE,
                JNI_FALSE);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

void WindowContextBase::process_mouse_cross(XCrossingEvent* event) {
    bool enter = event->type == EnterNotify;

    if (jview) {
        guint state = event->state;
        if (enter) { // workaround for RT-21590
            state &= ~MOUSE_BUTTONS_MASK;
        }

        if (enter != is_mouse_entered) {
            is_mouse_entered = enter;
            mainEnv->CallVoidMethod(jview, jViewNotifyMouse,
                    enter ? com_sun_glass_events_MouseEvent_ENTER : com_sun_glass_events_MouseEvent_EXIT,
                    com_sun_glass_events_MouseEvent_BUTTON_NONE,
                    (jint) event->x, (jint) event->y,
                    (jint) event->x_root, (jint) event->y_root,
                    xlib_modifier_mask_to_glass(state),
                    JNI_FALSE,
                    JNI_FALSE);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }
}

void WindowContextBase::process_key(GdkEventKey* event) {
//    bool press = event->type == GDK_KEY_PRESS;
//    jint glassKey = get_glass_key(event);
//    jint glassModifier = gdk_modifier_mask_to_glass(event->state);
//    if (press) {
//        glassModifier |= glass_key_to_modifier(glassKey);
//    } else {
//        glassModifier &= ~glass_key_to_modifier(glassKey);
//    }
//    jcharArray jChars = NULL;
//    jchar key = gdk_keyval_to_unicode(event->keyval);
//    if (key >= 'a' && key <= 'z' && (event->state & GDK_CONTROL_MASK)) {
//        key = key - 'a' + 1; // map 'a' to ctrl-a, and so on.
//    } else {
//#ifdef GLASS_GTK2
//        if (key == 0) {
//            // Work around "bug" fixed in gtk-3.0:
//            // http://mail.gnome.org/archives/commits-list/2011-March/msg06832.html
//            switch (event->keyval) {
//            case 0xFF08 /* Backspace */: key =  '\b';
//            case 0xFF09 /* Tab       */: key =  '\t';
//            case 0xFF0A /* Linefeed  */: key =  '\n';
//            case 0xFF0B /* Vert. Tab */: key =  '\v';
//            case 0xFF0D /* Return    */: key =  '\r';
//            case 0xFF1B /* Escape    */: key =  '\033';
//            case 0xFFFF /* Delete    */: key =  '\177';
//            }
//        }
//#endif
//    }
//
//    if (key > 0) {
//        jChars = mainEnv->NewCharArray(1);
//        if (jChars) {
//            mainEnv->SetCharArrayRegion(jChars, 0, 1, &key);
//            CHECK_JNI_EXCEPTION(mainEnv)
//        }
//    } else {
//        jChars = mainEnv->NewCharArray(0);
//    }
//    if (jview) {
//        if (press) {
//            mainEnv->CallVoidMethod(jview, jViewNotifyKey,
//                    com_sun_glass_events_KeyEvent_PRESS,
//                    glassKey,
//                    jChars,
//                    glassModifier);
//            CHECK_JNI_EXCEPTION(mainEnv)
//
//            if (jview && key > 0) { // TYPED events should only be sent for printable characters.
//                mainEnv->CallVoidMethod(jview, jViewNotifyKey,
//                        com_sun_glass_events_KeyEvent_TYPED,
//                        com_sun_glass_events_KeyEvent_VK_UNDEFINED,
//                        jChars,
//                        glassModifier);
//                CHECK_JNI_EXCEPTION(mainEnv)
//            }
//        } else {
//            mainEnv->CallVoidMethod(jview, jViewNotifyKey,
//                    com_sun_glass_events_KeyEvent_RELEASE,
//                    glassKey,
//                    jChars,
//                    glassModifier);
//            CHECK_JNI_EXCEPTION(mainEnv)
//        }
//    }
}

void WindowContextBase::paint(void* data, jint width, jint height) {
    Pixmap pixmap = XCreatePixmapFromBitmapData(display, DefaultRootWindow(display), (char *) data, width, height, 0, 0, depth);
    XCopyPlane(display, pixmap, xwindow, DefaultGC(display, DefaultScreen(display)), 0, 0, width, height, 0, 0, 1);

    XFlush(display);
    XFreePixmap(display, pixmap);

//    XImage* image = XCreateImage(display, visual, depth, ZPixmap, 0, (char*) data, width, height, 32, 0);
//    XPutImage(display, xwindow, DefaultGC(display, DefaultScreen(display)), image, 0, 0, 0, 0, width, height);
//    XDestroyImage(image);
}

void WindowContextBase::add_child(WindowContextTop* child) {
    children.insert(child);
//    gtk_window_set_transient_for(child->get_gtk_window(), this->get_gtk_window());
}

void WindowContextBase::remove_child(WindowContextTop* child) {
    children.erase(child);
//    gtk_window_set_transient_for(child->get_gtk_window(), NULL);
}

void WindowContextBase::show_or_hide_children(bool show) {
    std::set<WindowContextTop*>::iterator it;
    for (it = children.begin(); it != children.end(); ++it) {
        (*it)->set_minimized(!show);
        (*it)->show_or_hide_children(show);
    }
}

void WindowContextBase::reparent_children(WindowContext* parent) {
    std::set<WindowContextTop*>::iterator it;
    for (it = children.begin(); it != children.end(); ++it) {
        (*it)->set_owner(parent);
        parent->add_child(*it);
    }
    children.clear();
}

void WindowContextBase::set_visible(bool visible) {
    if (visible) {
        XFlush(display);
        g_print("XMapWindow\n");
        XMapWindow(display, xwindow);
    } else {
        XUnmapWindow(display, xwindow);
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
}

bool WindowContextBase::is_visible() {
    //VisibilityUnobscured, VisibilityPartiallyObscured, VisibilityFullyObscured
    return (visibility_state == VisibilityUnobscured
                || visibility_state == VisibilityPartiallyObscured);
}

bool WindowContextBase::set_view(jobject view) {

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

bool WindowContextBase::grab_mouse_drag_focus() {
//    if (glass_gdk_mouse_devices_grab_with_cursor(
//            gdk_window, gdk_window_get_cursor(gdk_window), FALSE)) {
//        WindowContextBase::sm_mouse_drag_window = this;
//        return true;
//    } else {
//        return false;
//    }
return true;
}

void WindowContextBase::ungrab_mouse_drag_focus() {
//    WindowContextBase::sm_mouse_drag_window = NULL;
//    glass_gdk_mouse_devices_ungrab();
//    if (WindowContextBase::sm_grab_window) {
//        WindowContextBase::sm_grab_window->grab_focus();
//    }
}

bool WindowContextBase::grab_focus() {
//    if (WindowContextBase::sm_mouse_drag_window
//            || glass_gdk_mouse_devices_grab(gdk_window)) {
//        WindowContextBase::sm_grab_window = this;
//        return true;
//    } else {
//        return false;
//    }
return true;
}

void WindowContextBase::ungrab_focus() {
//    if (!WindowContextBase::sm_mouse_drag_window) {
//        glass_gdk_mouse_devices_ungrab();
//    }
//    WindowContextBase::sm_grab_window = NULL;
//
//    if (jwindow) {
//        mainEnv->CallVoidMethod(jwindow, jWindowNotifyFocusUngrab);
//        CHECK_JNI_EXCEPTION(mainEnv)
//    }
}

void WindowContextBase::set_cursor(Cursor cursor) {
//TODO: XChangeActivePointerGrab.
//    if (!is_in_drag()) {
//        if (WindowContextBase::sm_mouse_drag_window) {
//            glass_gdk_mouse_devices_grab_with_cursor(
//                    WindowContextBase::sm_mouse_drag_window->get_gdk_window(), cursor, FALSE);
//        } else if (WindowContextBase::sm_grab_window) {
//            glass_gdk_mouse_devices_grab_with_cursor(
//                    WindowContextBase::sm_grab_window->get_gdk_window(), cursor, TRUE);
//        }
//    }
    XDefineCursor(display, xwindow, cursor);
}

void WindowContextBase::set_background(float r, float g, float b) {
#ifdef GLASS_GTK3
    GdkRGBA rgba = {0, 0, 0, 1.};
    rgba.red = r;
    rgba.green = g;
    rgba.blue = b;
//    gdk_window_set_background_rgba(gdk_window, &rgba);
#else
    GdkColor color;
    color.red   = (guint16) (r * 65535);
    color.green = (guint16) (g * 65535);
    color.blue  = (guint16) (b * 65535);
//    gtk_widget_modify_bg(gtk_widget, GTK_STATE_NORMAL, &color);
#endif
}

WindowContextBase::~WindowContextBase() {
    if (xim.ic) {
        XDestroyIC(xim.ic);
        xim.ic = NULL;
    }
    if (xim.im) {
        XCloseIM(xim.im);
        xim.im = NULL;
    }
    XDestroyWindow(display, xwindow);
}

////////////////////////////// WindowContextTop /////////////////////////////////
WindowContextTop::WindowContextTop(jobject _jwindow, WindowContext* _owner, long _screen,
        WindowFrameType _frame_type, WindowType type, int wmf) :
            WindowContextBase(),
            screen(_screen),
            frame_type(_frame_type),
            window_type(type),
            owner(_owner),
            geometry(),
            resizable(),
            frame_extents_initialized(),
            map_received(false),
            location_assigned(false),
            size_assigned(false),
            on_top(false),
            requested_bounds() {
    jwindow = mainEnv->NewGlobalRef(_jwindow);

    int mask = KeyPressMask
               | KeyReleaseMask
               | ButtonPressMask
               | ButtonReleaseMask
               | EnterWindowMask
               | LeaveWindowMask
               | PointerMotionMask
               | Button1MotionMask
               | Button2MotionMask
               | Button3MotionMask
               //Mouse Wheel
               | Button4MotionMask
               | Button5MotionMask
               | ButtonMotionMask
               | ExposureMask
               | VisibilityChangeMask
               | StructureNotifyMask
               | SubstructureNotifyMask
               | FocusChangeMask
               | PropertyChangeMask;

    display = X_CURRENT_DISPLAY;
    visual = DefaultVisual(display, DefaultScreen(display));
    glong xvisualID = (glong)mainEnv->GetStaticLongField(jApplicationCls, jApplicationVisualID);

    XVisualInfo* vinfo;
    XVisualInfo vinfo_template;
    depth = DefaultDepth(display, DefaultScreen(display));
    int visual_info_n = 0;
    if (xvisualID != 0) {
        vinfo_template.visualid = xvisualID;
        vinfo = XGetVisualInfo(display, VisualIDMask, &vinfo_template, &visual_info_n);

        if (visual_info_n > 0) {
            visual = vinfo[0].visual;
            depth = vinfo[0].depth;
        }
//        XFree(vinfo);
    }

    XSetWindowAttributes attr;
    attr.colormap = XCreateColormap(display, DefaultRootWindow(display), visual, AllocNone);
//    attr.border_pixel = 0;
    attr.background_pixel = (frame_type == TRANSPARENT)
                                ? 0x00000000
                                : WhitePixel(display, DefaultScreen(display));
    attr.override_redirect = (type == POPUP) ? True : False;
    attr.win_gravity = NorthWestGravity;
    attr.event_mask = mask;

    //TODO: child windows
    xparent = DefaultRootWindow(display);
    xwindow = XCreateWindow(display, xparent, 0, 0,
                            1, 1, 0, depth, InputOutput, visual,
                            CWEventMask  | CWOverrideRedirect | CWBitGravity |
                            CWColormap | CWBackPixel, &attr);

    if (XSaveContext(display, xwindow, X_CONTEXT, XPointer(this)) != 0) {
        g_print("Fail to save context\n");
    }

    Atom protocols[3] = { XInternAtom(display, "WM_DELETE_WINDOW", True),
                          XInternAtom(display, "WM_TAKE_FOCUS", True),
                          XInternAtom(display, "_NET_WM_PING", True) };
//                          XInternAtom(display, "_NET_WM_SYNC_REQUEST", True) };

    XSetWMProtocols(display, xwindow, protocols, 3);
    g_print("X WINDOW ID = %ld\n", xwindow);

    //TODO: set _NET_WM_PID

    Atom type_atom;
    switch (type) {
        case UTILITY:
            type_atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE_UTILITY", True);
            break;
        case POPUP:
            type_atom = XInternAtom(display, " _NET_WM_WINDOW_TYPE_POPUP_MENU", True);
            break;
        case NORMAL:
        default:
            XChangeProperty(display, xwindow, XInternAtom(display, "WM_CLIENT_LEADER", True),
                            XA_WINDOW, 32, PropModeReplace, (unsigned char *) &xwindow, 1);
            type_atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", True);
            break;
    }

    XChangeProperty(display, xwindow, XInternAtom(display, "_NET_WM_WINDOW_TYPE", True),
                   XA_ATOM, 32, PropModeReplace, (unsigned char *) &type_atom, 1);


    //for _NET_WM_PING
    long pid = getpid();
    XChangeProperty(display, xwindow,
                   XInternAtom(display, "_NET_WM_PID", True),
                   XA_CARDINAL, 32, PropModeReplace, (guchar *) & pid, 1);

    if (gchar* app_name = get_application_name()) {
        XClassHint *class_hints = XAllocClassHint();
        class_hints->res_name = app_name;
        class_hints->res_class = app_name;
        XSetClassHint(display, xwindow, class_hints);
        g_free(app_name);
        XFree(class_hints);
    }

    if (owner) {
//        owner->add_child(this);
//        if (on_top_inherited()) {
//            gtk_window_set_keep_above(GTK_WINDOW(gtk_widget), TRUE);
//        }
    }

    MwmHints hints;
    hints.functions = wmf;
    hints.flags = MWM_HINTS_FUNCTIONS;

    if (frame_type != TITLED) {
        hints.decorations = 0;
        hints.flags |= MWM_HINTS_DECORATIONS;
    }

    Atom motif_atom = XInternAtom(display, "_MOTIF_WM_HINTS", true);
    XChangeProperty(display, xwindow, motif_atom, motif_atom, 32,
                    PropModeReplace, (unsigned char *)&hints, sizeof (MwmHints)/sizeof (long));

    XFlush(display);

//    request_frame_extents();

//    gdk_window_register_dnd(gdk_window);
}

// Applied to a temporary full screen window to prevent sending events to Java
void WindowContextTop::detach_from_java() {
    if (jview) {
        mainEnv->DeleteGlobalRef(jview);
        jview = NULL;
    }
    if (jwindow) {
        mainEnv->DeleteGlobalRef(jwindow);
        jwindow = NULL;
    }
}

void WindowContextTop::activate_window() {
//    Atom navAtom = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
//    if (navAtom != None) {
//        XClientMessageEvent clientMessage;
//        memset(&clientMessage, 0, sizeof(clientMessage));
//
//        clientMessage.type = ClientMessage;
//        clientMessage.window = GDK_WINDOW_XID(gdk_window);
//        clientMessage.message_type = navAtom;
//        clientMessage.format = 32;
//        clientMessage.data.l[0] = 1;
//        clientMessage.data.l[1] = gdk_x11_get_server_time(gdk_window);
//        clientMessage.data.l[2] = 0;
//
//        XSendEvent(display, DefaultRootWindow(display), False,
//                   SubstructureRedirectMask | SubstructureNotifyMask,
//                   (XEvent *) &clientMessage);
//        XFlush(display);
//    }
}

//void WindowContextTop::request_frame_extents() {
//    if (frame_type != TITLED) {
//        return;
//    }
//
//    g_print("_NET_REQUEST_FRAME_EXTENTS\n");
//
//    Atom extents_request_atom = XInternAtom(display, "_NET_REQUEST_FRAME_EXTENTS", True);
//    XEvent xevent;
//
//    xevent.xclient.type = ClientMessage;
//    xevent.xclient.message_type = extents_request_atom;
//    xevent.xclient.display = display;
//    xevent.xclient.window = xwindow;
//    xevent.xclient.format = 32;
//    xevent.xclient.data.l[0] = 0;
//    xevent.xclient.data.l[1] = 0;
//    xevent.xclient.data.l[2] = 0;
//    xevent.xclient.data.l[3] = 0;
//    xevent.xclient.data.l[4] = 0;
//
//    XSendEvent(display, DefaultRootWindow(display), False,
//               SubstructureRedirectMask | SubstructureNotifyMask, &xevent);
//
//    XFlush(display);
//}

void WindowContextTop::update_frame_extents() {
    g_print("update_frame_extents()\n");
    Atom frame_extents_atom = XInternAtom(display, "_NET_FRAME_EXTENTS", True);
    bool changed = false;
    long top, left, bottom, right;

    Atom type_return;
    unsigned long nitems_return;
    unsigned long bytes_after_return;
    int format_return;
    unsigned char* data;

    if (XGetWindowProperty(display, xwindow, frame_extents_atom, 0, G_MAXLONG, False, XA_CARDINAL, &type_return,
                            &format_return, &nitems_return, &bytes_after_return, &data) == Success) {
        g_print("update_frame_extents()->got property, %d, %ld\n", format_return, nitems_return);
        if (type_return == XA_CARDINAL && format_return == 32 && nitems_return == 4 && data) {
            long* extents = (long *) data;
            left = extents[0];
            right = extents[1];
            top = extents[2];
            bottom = extents[3];

            g_print("Frame extents: %ld, %ld, %ld, %ld\n", top, left, bottom, right);

            changed = geometry.extents.top != top
                        || geometry.extents.left != left
                        || geometry.extents.bottom != bottom
                        || geometry.extents.right != right;
            if (changed) {
                geometry.extents.top = top;
                geometry.extents.left = left;
                geometry.extents.bottom = bottom;
                geometry.extents.right = right;

                update_window_constraints();
                notify_window_resize();
            }
        }
    }

    if(data) {
        XFree(data);
    }
}

static int geometry_get_window_width(const WindowGeometry *windowGeometry) {
     return (windowGeometry->final_width.type != BOUNDSTYPE_WINDOW)
                   ? windowGeometry->final_width.value
                         + windowGeometry->extents.left
                         + windowGeometry->extents.right
                   : windowGeometry->final_width.value;
}

static int geometry_get_window_height(const WindowGeometry *windowGeometry) {
    return (windowGeometry->final_height.type != BOUNDSTYPE_WINDOW)
                   ? windowGeometry->final_height.value
                         + windowGeometry->extents.top
                         + windowGeometry->extents.bottom
                   : windowGeometry->final_height.value;
}

static int geometry_get_content_width(WindowGeometry *windowGeometry) {
    return (windowGeometry->final_width.type != BOUNDSTYPE_CONTENT)
                   ? windowGeometry->final_width.value
                         - windowGeometry->extents.left
                         - windowGeometry->extents.right
                   : windowGeometry->final_width.value;
}
static int geometry_get_content_height(WindowGeometry *windowGeometry) {
    return (windowGeometry->final_height.type != BOUNDSTYPE_CONTENT)
                   ? windowGeometry->final_height.value
                         - windowGeometry->extents.top
                         - windowGeometry->extents.bottom
                   : windowGeometry->final_height.value;
}

static int geometry_get_window_x(const WindowGeometry *windowGeometry) {
    float value = windowGeometry->refx;
    if (windowGeometry->gravity_x != 0) {
        value -= geometry_get_window_width(windowGeometry)
                     * windowGeometry->gravity_x;
    }
    return (int) value;
}

static int geometry_get_window_y(const WindowGeometry *windowGeometry) {
    float value = windowGeometry->refy;
    if (windowGeometry->gravity_y != 0) {
        value -= geometry_get_window_height(windowGeometry)
                     * windowGeometry->gravity_y;
    }
    return (int) value;
}

static void geometry_set_window_x(WindowGeometry *windowGeometry, int value) {
    float newValue = value;
    if (windowGeometry->gravity_x != 0) {
        newValue += geometry_get_window_width(windowGeometry)
                * windowGeometry->gravity_x;
    }
    windowGeometry->refx = newValue;
}

static void geometry_set_window_y(WindowGeometry *windowGeometry, int value) {
    float newValue = value;
    if (windowGeometry->gravity_y != 0) {
        newValue += geometry_get_window_height(windowGeometry)
                * windowGeometry->gravity_y;
    }
    windowGeometry->refy = newValue;
}

void WindowContextTop::process_client_message(XClientMessageEvent* event) {
    g_print("process_client_message: %s\n", XGetAtomName(display, event->message_type));

    if (XInternAtom(display, "WM_PROTOCOLS", True) == event->message_type) {
        Atom atom = (Atom) event->data.l[0];
        g_print("WM_PROTOCOLS: %s\n", XGetAtomName(display, atom));

            if (XInternAtom(display, "_NET_WM_PING", True) == atom) {
            event->window = DefaultRootWindow(display);
            XSendEvent(display, event->window, False,
                      SubstructureRedirectMask | SubstructureNotifyMask, (XEvent *) event);
            XFlush(display);
        } else if (XInternAtom(display, "WM_TAKE_FOCUS", True) == atom) {
            XSetInputFocus(display, xwindow, RevertToParent, event->data.l[1]);
        } else if (XInternAtom(display, "WM_DELETE_WINDOW", True) == atom) {
            process_destroy();
        }
    }
}

void WindowContextTop::process_property(XPropertyEvent* event) {
    g_print("process_property: %s\n", XGetAtomName(display, event->atom));

    //TODO: global?
    Atom frame_extents_atom = XInternAtom(display, "_NET_FRAME_EXTENTS", True);
    Atom wm_state_atom = XInternAtom(display, "_NET_WM_STATE", True);

    if (event->state == PropertyNewValue && event->window == xwindow) {
        if (event->atom == frame_extents_atom) {
            update_frame_extents();
        } else if (event->atom == wm_state_atom) {
            //TODO
        }
    }
}

void WindowContextTop::process_configure(XConfigureEvent* event) {
    int x, y, w, h;

    if (frame_type == TITLED) {
        Window root;
        Window child;
        unsigned int ww, wh, wb, wd;
        int wx, wy;

        //TODO: may fail
        XGetGeometry(display, xwindow, &root, &wx, &wy, &ww, &wh, &wb, &wd);
        XTranslateCoordinates(display, xwindow, root, 0, 0, &wx, &wy, &child);

        x = wx;
        y = wy;
        w = ww;
        h = wh;
        geometry.current_width = ww;
        geometry.current_height = wh;
    } else {
        x = event->x;
        y = event->y;
        w = event->width;
        h = event->height;
    }
    g_print("process_configure: %d, %d, %d, %d\n", x, y, w, h);

    geometry.final_width.value = w;
    geometry.final_width.type = BOUNDSTYPE_CONTENT;
    geometry.final_height.value = h;
    geometry.final_height.type = BOUNDSTYPE_CONTENT;

    geometry_set_window_x(&geometry, x);
    geometry_set_window_y(&geometry, y);

    if (jwindow) {
        notify_window_resize();
        notify_window_move();
    }

    glong to_screen = getScreenPtrForLocation(x, y);
    if (to_screen != -1) {
        if (to_screen != screen) {
            if (jwindow) {
                //notify screen changed
                jobject jScreen = createJavaScreen(mainEnv, to_screen);
                mainEnv->CallVoidMethod(jwindow, jWindowNotifyMoveToAnotherScreen, jScreen);
                CHECK_JNI_EXCEPTION(mainEnv)
            }
            screen = to_screen;
        }
    }
}

void WindowContextTop::update_window_constraints() {
    g_print("update_window_constraints\n");
    XSizeHints* hints = XAllocSizeHints();

    hints->win_gravity = NorthWestGravity;
    hints->flags = (PMinSize | PMaxSize | PWinGravity);

    if (resizable.value) {
        int min_w = (resizable.minw == -1) ? 1
                      : resizable.minw - geometry.extents.left - geometry.extents.right;
        int min_h =  (resizable.minh == -1) ? 1
                      : resizable.minh - geometry.extents.top - geometry.extents.bottom;

        hints->min_width = (min_w < 1) ? 1 : min_w;
        hints->min_height = (min_h < 1) ? 1 : min_h;

        hints->max_width = (resizable.maxw == -1) ? G_MAXINT
                            : resizable.maxw - geometry.extents.left - geometry.extents.right;

        hints->max_height = (resizable.maxh == -1) ? G_MAXINT
                           : resizable.maxh - geometry.extents.top - geometry.extents.bottom;

        g_print("min/max size (resizable) %d,%d/%d,%d\n", hints->min_width, hints->min_height,
            hints->max_width, hints->max_height);

    } else {
        int w = geometry_get_content_width(&geometry);
        int h = geometry_get_content_height(&geometry);

        g_print("min/max size (not resizable) %d,%d\n", w, h);
        hints->min_width = w;
        hints->min_height = h;
        hints->max_width = w;
        hints->max_height = h;
    }

    if (!map_received) {
        hints->x = geometry_get_window_x(&geometry);
        hints->y = geometry_get_window_y(&geometry);
        hints->flags |= PPosition;
    }

    XSetWMNormalHints(display, xwindow, hints);
    XFree(hints);
}

void WindowContextTop::set_resizable(bool res) {
    resizable.value = res;
    update_window_constraints();
}

void WindowContextTop::set_visible(bool visible) {
    WindowContextBase::set_visible(visible);

    //JDK-8220272 - fire event first because GDK_FOCUS_CHANGE is not always in order
//    if (visible && jwindow && isEnabled()) {
//        mainEnv->CallVoidMethod(jwindow, jWindowNotifyFocus, com_sun_glass_events_WindowEvent_FOCUS_GAINED);
//        CHECK_JNI_EXCEPTION(mainEnv);
//    }
}

void WindowContextTop::set_bounds(int x, int y, bool xSet, bool ySet, int w, int h, int cw, int ch) {
    g_print("set_bounds: %d, %d, %d, %d, %d, %d\n", x, y, w, h, cw, ch);

    requested_bounds.width = w;
    requested_bounds.height = h;
    requested_bounds.client_width = cw;
    requested_bounds.client_height = ch;

    XWindowChanges windowChanges;
    unsigned int windowChangesMask = 0;
    if (w > 0) {
        geometry.final_width.value = w;
        geometry.final_width.type = BOUNDSTYPE_WINDOW;
        geometry.current_width = geometry_get_window_width(&geometry);
        windowChanges.width = geometry_get_content_width(&geometry);
        windowChangesMask |= CWWidth;
    } else if (cw > 0) {
        geometry.final_width.value = cw;
        geometry.final_width.type = BOUNDSTYPE_CONTENT;
        geometry.current_width = geometry_get_window_width(&geometry);
        windowChanges.width = geometry_get_content_width(&geometry);
        windowChangesMask |= CWWidth;
    }

    if (h > 0) {
        geometry.final_height.value = h;
        geometry.final_height.type = BOUNDSTYPE_WINDOW;
        geometry.current_height = geometry_get_window_height(&geometry);
        windowChanges.height = geometry_get_content_height(&geometry);
        windowChangesMask |= CWHeight;
    } else if (ch > 0) {
        geometry.final_height.value = ch;
        geometry.final_height.type = BOUNDSTYPE_CONTENT;
        geometry.current_height = geometry_get_window_height(&geometry);
        windowChanges.height = geometry_get_content_height(&geometry);
        windowChangesMask |= CWHeight;
    }

    if (xSet || ySet) {
        if (xSet) {
            geometry.refx = x + geometry.current_width * geometry.gravity_x;
        }

        windowChanges.x = geometry_get_window_x(&geometry);
        windowChangesMask |= CWX;

        if (ySet) {
            geometry.refy = y + geometry.current_height * geometry.gravity_y;
        }

        windowChanges.y = geometry_get_window_y(&geometry);
        windowChangesMask |= CWY;

        location_assigned = true;
    }

    if (w > 0 || h > 0 || cw > 0 || ch > 0) size_assigned = true;

    window_configure(&windowChanges, windowChangesMask);
}

void WindowContextTop::process_map() {
    map_received = true;
}

void WindowContextTop::window_configure(XWindowChanges *windowChanges, unsigned int windowChangesMask) {
    if (windowChangesMask == 0) {
        return;
    }

    update_window_constraints();

    if (windowChangesMask & (CWX | CWY)) {
        if (!map_received) {
            notify_window_move();
        }

        g_print("(windowChangesMask & (CWX | CWY): %d, %d\n", windowChanges->x, windowChanges->y);
    }

    if (windowChangesMask & (CWWidth | CWHeight)) {
        if (!map_received) {
            notify_window_resize();
        }
    }

    XReconfigureWMWindow(display, xwindow, 0, windowChangesMask, windowChanges);
    XFlush(display);
}

void WindowContextTop::applyShapeMask(void* data, uint width, uint height)
{
    if (frame_type != TRANSPARENT) {
        return;
    }

//    glass_window_apply_shape_mask(gtk_widget_get_window(gtk_widget), data, width, height);
}

void WindowContextTop::set_minimized(bool minimize) {
    is_iconified = minimize;

    change_wm_state(minimize,
                    XInternAtom(display, "_NET_WM_STATE_HIDDEN", true), None);
}

void WindowContextTop::set_maximized(bool maximize) {
    is_maximized = maximize;

    change_wm_state(maximize,
                    XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", true),
                    XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", true));
}

void WindowContextTop::change_wm_state(bool add, Atom state1, Atom state2) {
    XClientMessageEvent xclient;

    memset(&xclient, 0, sizeof (xclient));
    xclient.type = ClientMessage;
    xclient.window = xwindow;
    xclient.message_type = XInternAtom(display, "_NET_WM_STATE", true);
    xclient.format = 32;
    xclient.data.l[0] = add ? 1 : 0;
    xclient.data.l[1] = state1;
    xclient.data.l[2] = state2;
    xclient.data.l[3] = 1; /* source indication */
    xclient.data.l[4] = 0;

    XSendEvent(display, DefaultRootWindow(display), False,
                SubstructureRedirectMask | SubstructureNotifyMask, (XEvent *)&xclient);
}

void WindowContextTop::enter_fullscreen() {
    //FIXME: may have to set this if unmapped
//    Atom atoms[1];
//    atoms[0] = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", true);
//
//    XChangeProperty(display, xwindow, XInternAtom(display, "_NET_WM_STATE", true),
//                    XA_WINDOW, 32, PropModeReplace, (unsigned char *) atoms, 1);
    change_wm_state(true, XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", true), None);
}

void WindowContextTop::exit_fullscreen() {
    change_wm_state(false, XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", true), None);
//    XDeleteProperty(display, xwindow, XInternAtom(display, "_NET_WM_STATE", true));
}

void WindowContextTop::request_focus() {
//    XRaiseWindow(display, xwindow);
//RevertToParent, RevertToPointerRoot, or RevertToNone
    XSetInputFocus(display, xwindow, RevertToParent, CurrentTime);
}

void WindowContextTop::set_focusable(bool focusable) {
    XWMHints *hints = XAllocWMHints();
    hints->input = (focusable) ? True : False;
    hints->flags = InputHint;
    XSetWMHints(display, xwindow, hints);

    XFree(hints);
}

void WindowContextTop::set_title(const char* title) {
    XSetWMName(display, xwindow, (XTextProperty *) title);

    XChangeProperty(display, xwindow,
                   XInternAtom(display, "_NET_WM_NAME", true),
                   XInternAtom(display, "UTF8_STRING", true), 8,
                   PropModeReplace, (unsigned char*) title, strlen (title));
}

void WindowContextTop::set_alpha(double alpha) {
//    /gtk_window_set_opacity(GTK_WINDOW(gtk_widget), (gdouble)alpha);
}

void WindowContextTop::set_enabled(bool enabled) {

}

void WindowContextTop::set_minimum_size(int w, int h) {
    resizable.minw = (w <= 0) ? 1 : w;
    resizable.minh = (h <= 0) ? 1 : h;
    update_window_constraints();
}

void WindowContextTop::set_maximum_size(int w, int h) {
    resizable.maxw = w;
    resizable.maxh = h;
    update_window_constraints();
}

void WindowContextTop::set_icon(GdkPixbuf* pixbuf) {
//    gtk_window_set_icon(GTK_WINDOW(gtk_widget), pixbuf);
}

void WindowContextTop::restack(bool restack) {
//    gdk_window_restack(gdk_window, NULL, restack ? TRUE : FALSE);
}

void WindowContextTop::set_modal(bool modal, WindowContext* parent) {
    //Currently not used
}

WindowFrameExtents WindowContextTop::get_frame_extents() {
    return geometry.extents;
}

void WindowContextTop::set_gravity(float x, float y) {
    int oldX = geometry_get_window_x(&geometry);
    int oldY = geometry_get_window_y(&geometry);
    geometry.gravity_x = x;
    geometry.gravity_y = y;
    geometry_set_window_x(&geometry, oldX);
    geometry_set_window_y(&geometry, oldY);
}

void WindowContextTop::update_ontop_tree(bool on_top) {
    bool effective_on_top = on_top || this->on_top;
//    gtk_window_set_keep_above(GTK_WINDOW(gtk_widget), effective_on_top ? TRUE : FALSE);
    for (std::set<WindowContextTop*>::iterator it = children.begin(); it != children.end(); ++it) {
        (*it)->update_ontop_tree(effective_on_top);
    }
}

bool WindowContextTop::on_top_inherited() {
    WindowContext* o = owner;
    while (o) {
        WindowContextTop* topO = dynamic_cast<WindowContextTop*>(o);
        if (!topO) break;
        if (topO->on_top) {
            return true;
        }
        o = topO->owner;
    }
    return false;
}

bool WindowContextTop::effective_on_top() {
    if (owner) {
        WindowContextTop* topO = dynamic_cast<WindowContextTop*>(owner);
        return (topO && topO->effective_on_top()) || on_top;
    }
    return on_top;
}

void WindowContextTop::notify_on_top(bool top) {
    // Do not report effective (i.e. native) values to the FX, only if the user sets it manually
    if (top != effective_on_top() && jwindow) {
        if (on_top_inherited() && !top) {
            // Disallow user's "on top" handling on windows that inherited the property
//            gtk_window_set_keep_above(GTK_WINDOW(gtk_widget), TRUE);
        } else {
            on_top = top;
            update_ontop_tree(top);
            mainEnv->CallVoidMethod(jwindow,
                    jWindowNotifyLevelChanged,
                    top ? com_sun_glass_ui_Window_Level_FLOATING :  com_sun_glass_ui_Window_Level_NORMAL);
            CHECK_JNI_EXCEPTION(mainEnv);
        }
    }
}

void WindowContextTop::notify_window_resize() {
    int w = geometry_get_window_width(&geometry);
    int h = geometry_get_window_height(&geometry);

    g_print("jWindowNotifyResize -> %d,%d\n", w, h);

    mainEnv->CallVoidMethod(jwindow, jWindowNotifyResize,
            (is_maximized)
                ? com_sun_glass_events_WindowEvent_MAXIMIZE
                : com_sun_glass_events_WindowEvent_RESIZE,
            w, h);
    CHECK_JNI_EXCEPTION(mainEnv)

    if (jview) {
        int cw = geometry_get_content_width(&geometry);
        int ch = geometry_get_content_height(&geometry);

        g_print("jViewNotifyResize %d, %d\n", cw, ch);
        mainEnv->CallVoidMethod(jview, jViewNotifyResize, cw, ch);
        CHECK_JNI_EXCEPTION(mainEnv)

//        mainEnv->CallVoidMethod(jview, jViewNotifyView,
//                com_sun_glass_events_ViewEvent_MOVE);
//        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

void WindowContextTop::notify_window_move() {
    int x = geometry_get_window_x(&geometry);
    int y = geometry_get_window_y(&geometry);

    g_print("jWindowNotifyMove %d, %d\n", x, y);

    mainEnv->CallVoidMethod(jwindow, jWindowNotifyMove, x, y);
    CHECK_JNI_EXCEPTION(mainEnv)

    if (jview) {
        mainEnv->CallVoidMethod(jview, jViewNotifyView,
                com_sun_glass_events_ViewEvent_MOVE);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

void WindowContextTop::set_level(int level) {
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

void WindowContextTop::set_owner(WindowContext * owner_ctx) {
    owner = owner_ctx;
}

void WindowContextTop::process_destroy() {
    if (owner) {
        owner->remove_child(this);
    }

    WindowContextBase::process_destroy();
}
