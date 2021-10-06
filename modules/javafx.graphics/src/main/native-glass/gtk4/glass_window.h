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
#ifndef GLASS_WINDOW_H
#define        GLASS_WINDOW_H

#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <cairo.h>

#include <jni.h>
#include <set>
#include <vector>

#include "glass_view.h"
#include "glass_general.h"

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

enum request_type {
    REQUEST_NONE,
    REQUEST_RESIZABLE,
    REQUEST_NOT_RESIZABLE
};

static const guint MOUSE_BUTTONS_MASK = (guint)(GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK);

struct BgColor {
    BgColor() : red(0), green(0), blue(0), is_set(FALSE) {}

    float red;
    float green;
    float blue;
    bool is_set;
};

struct WindowGeometry {
    WindowGeometry() : current_x(0),
                       current_y(0),
                       current_w(0),
                       current_h(0),
                       current_cw(0),
                       current_ch(0),
                       last_cw(0),
                       last_ch(0),
                       view_x(0),
                       view_y(0),
                       enabled(true),
                       resizable(true),
                       minw(-1),
                       minh(-1),
                       maxw(-1),
                       maxh(-1) {}

    int current_x; // current position X
    int current_y; // current position Y
    int current_w; // current window width, adjusted
    int current_h; // current window height, adjusted
    int current_cw; // current content (view) width
    int current_ch; // current content (view) height
    int last_cw; // not subjected to fullscreen / maximize
    int last_ch;

    // The position of the view relative to the window
    int view_x;
    int view_y;

    bool enabled;
    bool resizable;

    int minw;
    int minh;

    int maxw;
    int maxh;
};

class WindowContext {
private:
    jlong screen;
    WindowFrameType frame_type;
    WindowType window_type;
    struct WindowContext *owner;
    jobject jwindow;
    jobject jview;

    bool map_received;
    bool visible_received;
    bool on_top;
    bool is_fullscreen;
    bool is_iconified;
    bool is_maximized;
    bool is_mouse_entered;
    bool can_be_deleted;

    struct _XIM {
    _XIM() : im(NULL), ic(NULL), enabled(FALSE) {}
        XIM im;
        XIC ic;
        bool enabled;
    } xim;

    size_t events_processing_cnt;

    WindowGeometry geometry;
    std::set<WindowContext *> children;
    cairo_surface_t *cairo_surface;
    GtkWidget *gtk_widget;
    GtkWidget *drawing_area;
    BgColor bg_color;

    static WindowContext* sm_mouse_drag_window;
    static WindowContext* sm_grab_window;
public:
    WindowContext(jobject, WindowContext *, long, WindowFrameType, WindowType);

    bool hasIME();
    bool filterIME(GdkEvent *);
    void enableOrResetIME();
    void disableIME();

    void paint(void*, jint, jint);
    bool isEnabled();

    GdkSurface *get_gdk_surface();
    cairo_surface_t *get_cairo_surface();
    GtkWidget *get_gtk_widget();
    GtkWindow *get_gtk_window();
    WindowGeometry get_geometry();
    jobject get_jwindow();
    jobject get_jview();

    void process_map();
    void process_focus(GdkEvent*);
//    void process_property_notify(GdkPropertyEvent *);
    void process_configure();
    void process_destroy();
    void process_delete();
//    void process_expose(GdkEventExpose*);
    void process_mouse_button(GdkEvent*);
    void process_mouse_motion(GdkEvent*);
    void process_mouse_scroll(GdkEvent*);
    void process_mouse_cross(GdkEvent*);
    void process_key(GdkEvent*);
//    void process_state(GdkEventWindowState*);
    void process_screen_changed();

    void notify_on_top(bool);
    void notify_repaint();
    void notify_state(jint);

    bool set_view(jobject);
    void set_visible(bool);
    void set_cursor(GdkCursor*);
    void set_level(int);
    void set_background(float, float, float);
    void set_minimized(bool);
    void set_maximized(bool);
    void set_bounds(int, int, bool, bool, int, int, int, int);
    void set_resizable(bool);
    void set_focusable(bool);
    void set_title(const char *);
    void set_alpha(double);
    void set_enabled(bool);
    void set_minimum_size(int, int);
    void set_maximum_size(int, int);
    void set_icon(GdkPixbuf *);
    void set_modal(bool, WindowContext *parent = NULL);
    void set_gravity(float, float);
    void set_owner(WindowContext *);
    void add_child(WindowContext *);
    void remove_child(WindowContext *);
    void show_or_hide_children(bool);
    bool is_visible();
    bool is_dead();
    void restack(bool);
    void request_focus();
    void enter_fullscreen();
    void exit_fullscreen();
    void detach_from_java();
    void increment_events_counter();
    void decrement_events_counter();
    size_t get_events_count();
    ~WindowContext();

private:
    bool im_filter_keypress(GdkKeyEvent*);
    void size_position_notify(bool, bool);
    void update_ontop_tree(bool);
    bool on_top_inherited();
    bool effective_on_top();
};

void destroy_and_delete_ctx(WindowContext *ctx);

class EventsCounterHelper {
private:
    WindowContext *ctx;
public:
    explicit EventsCounterHelper(WindowContext *context) {
        ctx = context;
        ctx->increment_events_counter();
    }

    ~EventsCounterHelper() {
        ctx->decrement_events_counter();
        if (ctx->is_dead() && ctx->get_events_count() == 0) {
            delete ctx;
        }
        ctx = NULL;
    }
};

#endif        /* GLASS_WINDOW_H */

