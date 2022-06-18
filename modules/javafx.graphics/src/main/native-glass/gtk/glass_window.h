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
#ifndef GLASS_WINDOW_H
#define        GLASS_WINDOW_H


#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/shape.h>
#include <cairo/cairo-xlib.h>
#include <glib.h>

#include <jni.h>
#include <set>
#include <vector>

#include "glass_view.h"

#define MWM_FUNC_RESIZE         (1L << 1)
#define MWM_FUNC_MOVE           (1L << 2)
#define MWM_FUNC_MINIMIZE       (1L << 3)
#define MWM_FUNC_MAXIMIZE       (1L << 4)
#define MWM_FUNC_CLOSE          (1L << 5)

#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)

enum WindowFrameType {
    TITLED,
    UNTITLED,
    TRANSPARENT
};

enum WindowType {
    NORMAL,
    UTILITY,
    POPUP
};

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
} MwmHints;

struct WindowFrameExtents {
    int top;
    int left;
    int bottom;
    int right;
};

static const int MOUSE_BUTTONS_MASK = Button1Mask | Button2Mask | Button3Mask;

static const int WINDOW_EVENT_MASK = KeyPressMask
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

enum BoundsType {
    BOUNDSTYPE_CONTENT,
    BOUNDSTYPE_WINDOW
};

struct WindowGeometry {
    WindowGeometry(): final_width(), final_height(),
    refx(), refy(), gravity_x(), gravity_y(), current_width(), current_height(), extents() {}
    // estimate of the final width the window will get after all pending
    // configure requests are processed by the window manager
    struct {
        int value;
        BoundsType type;
    } final_width;

    struct {
        int value;
        BoundsType type;
    } final_height;

    float refx;
    float refy;
    float gravity_x;
    float gravity_y;

    // the last width which was configured or obtained from configure
    // notification
    int current_width;

    // the last height which was configured or obtained from configure
    // notification
    int current_height;

    WindowFrameExtents extents;
};

class WindowContext {
private:

    size_t events_processing_cnt;
    jlong screen;
    WindowFrameType frame_type;
    WindowType window_type;
    struct WindowContext *owner;
    WindowGeometry geometry;

    bool can_be_deleted;
    std::set<WindowContext*> children;
    jobject jwindow;
    jobject jview;
    Window xwindow;
    Window xparent;
    Display* display;
    XVisualInfo vinfo;

    int visibility_state;
    bool is_iconified;
    bool is_maximized;
    bool is_mouse_entered;
    bool is_disabled;

    /*
     * sm_grab_window points to WindowContext holding a mouse grab.
     * It is mostly used for popup windows.
     */
    static WindowContext* sm_grab_window;

    /*
     * sm_mouse_drag_window points to a WindowContext from which a mouse drag started.
     * This WindowContext holding a mouse grab during this drag. After releasing
     * all mouse buttons sm_mouse_drag_window becomes NULL and sm_grab_window's
     * mouse grab should be restored if present.
     *
     * This is done in order to mimic Windows behavior:
     * All mouse events should be delivered to a window from which mouse drag
     * started, until all mouse buttons released. No mouse ENTER/EXIT events
     * should be reported during this drag.
     */
    static WindowContext* sm_mouse_drag_window;

    struct _XIM {
        _XIM() : im(NULL), ic(NULL), enabled(FALSE) {}
        XIM im;
        XIC ic;
        bool enabled;
    } xim;
    struct _Resizable{// we can't use set/get gtk_window_resizable function
        _Resizable(): value(true), minw(-1), minh(-1), maxw(-1), maxh(-1) {}
        bool value; //actual value of resizable for a window
        int minw, minh, maxw, maxh; //minimum and maximum window width/height;
    } resizable;

    bool frame_extents_initialized;
    bool map_received;
    bool location_assigned;
    bool size_assigned;
    bool on_top;

    struct _Size {
        int width, height;
        int client_width, client_height;
    } requested_bounds;

    bool is_null_extents() { return is_null_extents(geometry.extents); }

    bool is_null_extents(WindowFrameExtents ex) {
        return !ex.top && !ex.left && !ex.bottom && !ex.right;
    }

    static WindowFrameExtents normal_extents;
    static WindowFrameExtents utility_extents;

public:
    WindowContext(jobject, WindowContext*, long, WindowFrameType, WindowType, int);
    bool isEnabled();
    bool hasIME();
    bool filterIME(XEvent *);
    void enableOrResetIME();
    void disableIME();
    void paint(void*, jint, jint);
    Window get_window();
    jobject get_jwindow();
    jobject get_jview();

    void add_child(WindowContext*);
    void remove_child(WindowContext*);
    void show_or_hide_children(bool);
    void reparent_children(WindowContext* parent);
    void set_visible(bool);
    bool is_visible();
    bool set_view(jobject);
    bool grab_focus();
    bool grab_mouse_drag_focus();
    void ungrab_focus();
    void ungrab_mouse_drag_focus();
    void set_cursor(Cursor);
    void set_background(float, float, float);

    void process_focus(XFocusChangeEvent*);
    void process_delete();
    void process_expose(XExposeEvent *);
    void process_damage(XDamageNotifyEvent *);
    void process_mouse_button(XButtonEvent*);
    void process_mouse_motion(XMotionEvent*);
    void process_mouse_cross(XCrossingEvent*);
    void process_key(XKeyEvent*);
    void process_visibility(XVisibilityEvent*);
    void process_map();
    void process_property(XPropertyEvent*);
    void process_client_message(XClientMessageEvent*);
    void process_configure(XConfigureEvent*);
    void process_destroy();
    void notify_state(jint);

    void increment_events_counter();
    void decrement_events_counter();
    size_t get_events_count();
    bool is_dead();

    void change_wm_state(bool add, Atom state1, Atom state2);

    WindowFrameExtents get_frame_extents();

    void set_minimized(bool);
    void set_maximized(bool);
    void set_bounds(int, int, bool, bool, int, int, int, int);
    void set_resizable(bool);
    void request_focus();
    void set_focusable(bool);
    void set_title(const char*);
    void set_alpha(double);
    void set_enabled(bool);
    void set_minimum_size(int, int);
    void set_maximum_size(int, int);
    void set_icon(cairo_surface_t*);
    void restack(bool);
    void set_modal(bool, WindowContext* parent = NULL);
    void set_gravity(float, float);
    void set_level(int);
    void notify_on_top(bool);
    void notify_window_resize();
    void notify_window_move();

    void enter_fullscreen();
    void exit_fullscreen();

    void set_owner(WindowContext*);

    void detach_from_java();

    ~WindowContext();
private:
    bool im_filter_keypress(XKeyEvent*);
    bool get_frame_extents_property(int *, int *, int *, int *);
    void activate_window();
    void update_frame_extents();
//    void request_frame_extents();
    void window_configure(XWindowChanges *, unsigned int);
    void update_window_constraints();
};

void destroy_and_delete_ctx(WindowContext* ctx);

class EventsCounterHelper {
private:
    WindowContext* ctx;
public:
    explicit EventsCounterHelper(WindowContext* context) {
        ctx = context;
        ctx->increment_events_counter();
    }
    ~EventsCounterHelper() {
        ctx->decrement_events_counter();
        if (ctx->is_dead() && ctx->get_events_count() == 0) {
           g_print("event count helper\n");
           delete ctx;
        }
        ctx = NULL;
    }
};

#endif        /* GLASS_WINDOW_H */

