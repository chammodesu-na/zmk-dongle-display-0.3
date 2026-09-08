/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>
#include <zmk/endpoints.h>

#include "output_status.h"
#if IS_ENABLED(CONFIG_ZMK_BLE)
#  include <zmk/events/ble_active_profile_changed.h>
#  include <zmk/ble.h>
#endif

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* The active endpoint is spelled out in the large font ("USB", "BT1" ... "BT5")
 * and the link state is shown as a dot next to it:
 *   filled dot  -> connected / HID ready
 *   thick ring  -> paired, but currently not connected
 *   thin ring   -> profile is open (not paired yet)
 */
#define STATUS_DOT_SIZE 11

enum output_child {
    output_child_label,
    output_child_dot,
};

enum link_state {
    link_state_connected,
    link_state_disconnected,
    link_state_open,
};

struct output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    bool usb_is_hid_ready;
};

static struct output_status_state get_state(const zmk_event_t *_eh) {
    struct output_status_state st;

    st.selected_endpoint = zmk_endpoints_selected();

#if IS_ENABLED(CONFIG_ZMK_BLE)
    st.active_profile_index     = zmk_ble_active_profile_index();
    st.active_profile_connected = zmk_ble_active_profile_is_connected();
    st.active_profile_bonded    = !zmk_ble_active_profile_is_open();
#else
    st.active_profile_index     = 0;
    st.active_profile_connected = false;
    st.active_profile_bonded    = false;
#endif

    st.usb_is_hid_ready = zmk_usb_is_hid_ready();
    return st;
}

static void set_link_state(lv_obj_t *dot, enum link_state state) {
    switch (state) {
    case link_state_connected:
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        break;
    case link_state_disconnected:
        lv_obj_set_style_bg_opa(dot, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(dot, 3, 0);
        break;
    case link_state_open:
        lv_obj_set_style_bg_opa(dot, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(dot, 1, 0);
        break;
    }
}

static void set_status_symbol(lv_obj_t *widget, struct output_status_state state) {
    lv_obj_t *label = lv_obj_get_child(widget, output_child_label);
    lv_obj_t *dot = lv_obj_get_child(widget, output_child_dot);

    switch (state.selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        lv_label_set_text(label, "USB");
        set_link_state(dot, state.usb_is_hid_ready ? link_state_connected
                                                   : link_state_disconnected);
        break;
    case ZMK_TRANSPORT_BLE: {
        char text[8];
        snprintf(text, sizeof(text), "BT%d", state.active_profile_index + 1);
        lv_label_set_text(label, text);

        if (!state.active_profile_bonded) {
            set_link_state(dot, link_state_open);
        } else {
            set_link_state(dot, state.active_profile_connected ? link_state_connected
                                                               : link_state_disconnected);
        }
        break;
    }
    default:
        lv_label_set_text(label, "---");
        set_link_state(dot, link_state_open);
        break;
    }
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_output_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_status_symbol(widget->obj, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);

int zmk_widget_output_status_init(struct zmk_widget_output_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(widget->obj);
    lv_obj_set_size(widget->obj, 24 + 3 + STATUS_DOT_SIZE, 16);

    lv_obj_t *label = lv_label_create(widget->obj);
    lv_obj_set_style_text_font(label, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(label, "---");

    lv_obj_t *dot = lv_obj_create(widget->obj);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, STATUS_DOT_SIZE, STATUS_DOT_SIZE);
    lv_obj_align(dot, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_white(), 0);
    lv_obj_set_style_border_color(dot, lv_color_white(), 0);
    set_link_state(dot, link_state_open);

    sys_slist_append(&widgets, &widget->node);

    widget_output_status_init();
    return 0;
}

lv_obj_t *zmk_widget_output_status_obj(struct zmk_widget_output_status *widget) {
    return widget->obj;
}
