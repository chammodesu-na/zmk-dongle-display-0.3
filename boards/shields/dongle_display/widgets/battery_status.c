/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/usb.h>

#include "battery_status.h"

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
    #define SOURCE_OFFSET 1
#else
    #define SOURCE_OFFSET 0
#endif

#ifndef ZMK_SPLIT_BLE_PERIPHERAL_COUNT
#  define ZMK_SPLIT_BLE_PERIPHERAL_COUNT 0
#endif

#define BATTERY_SOURCES (ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET)

/* One row per battery: name, gauge bar, percentage.
 *
 *   L |||||||||     87%
 *
 * The bar is the part that is readable at a glance, the number is there for
 * when you actually care about the exact value.
 */
#define ROW_NAME_W   9
#define ROW_BAR_X    (ROW_NAME_W + 2)
#define ROW_BAR_W    73
#define ROW_BAR_H    9
#define ROW_PCT_W    36
#define ROW_PITCH    (BATTERY_SOURCES >= 3 ? 9 : 12)

/* Leave a 1px gap inside the outline so the fill never touches the border. */
#define BAR_FILL_MAX (ROW_BAR_W - 4)

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct battery_state {
    uint8_t source;
    uint8_t level;
    bool usb_present;
};

struct battery_object {
    lv_obj_t *name;
    lv_obj_t *bar;
    lv_obj_t *fill;
    lv_obj_t *label;
} battery_objects[BATTERY_SOURCES];

/* Source 0 is the dongle when its battery is shown, the peripherals follow.
 * With the usual two halves they are labelled L and R, otherwise numbered.
 */
static const char *source_name(int index) {
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
    if (index == 0) {
        return "D";
    }
#endif
    switch (index - SOURCE_OFFSET) {
    case 0:
        return ZMK_SPLIT_BLE_PERIPHERAL_COUNT == 2 ? "L" : "1";
    case 1:
        return ZMK_SPLIT_BLE_PERIPHERAL_COUNT == 2 ? "R" : "2";
    case 2:
        return "3";
    case 3:
        return "4";
    default:
        return "?";
    }
}

static void set_battery_symbol(lv_obj_t *widget, struct battery_state state) {
    if (state.source >= BATTERY_SOURCES) {
        return;
    }
    LOG_DBG("source: %d, level: %d, usb: %d", state.source, state.level, state.usb_present);

    struct battery_object *obj = &battery_objects[state.source];

    uint8_t level = state.level > 100 ? 100 : state.level;
    lv_coord_t fill_w = (BAR_FILL_MAX * level) / 100;

    lv_obj_set_width(obj->fill, fill_w);
    if (fill_w > 0) {
        lv_obj_clear_flag(obj->fill, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj->fill, LV_OBJ_FLAG_HIDDEN);
    }

    if (state.usb_present) {
        lv_label_set_text(obj->label, "CHG");
    } else {
        lv_label_set_text_fmt(obj->label, "%d%%", level);
    }

    /* A source that has never reported anything stays blank instead of
     * claiming 0%. */
    if (state.level > 0 || state.usb_present) {
        lv_obj_clear_flag(obj->name, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(obj->bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(obj->label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj->name, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(obj->bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(obj->label, LV_OBJ_FLAG_HIDDEN);
    }
}

void battery_status_update_cb(struct battery_state state) {
    struct zmk_widget_dongle_battery_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_symbol(widget->obj, state); }
}

static struct battery_state peripheral_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev = as_zmk_peripheral_battery_state_changed(eh);

    // ★★★ 변경: 왼쪽(1)과 오른쪽(0) 순서 반전 ★★★
    uint8_t reversed_source = (ev->source == 0) ? 1 : 0;

    return (struct battery_state){
        .source = reversed_source + SOURCE_OFFSET,
        .level = ev->state_of_charge,
    };
}

static struct battery_state central_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct battery_state) {
        .source = 0,
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

static struct battery_state battery_status_get_state(const zmk_event_t *eh) { 
    if (as_zmk_peripheral_battery_state_changed(eh) != NULL) {
        return peripheral_battery_status_get_state(eh);
    } else {
        return central_battery_status_get_state(eh);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_dongle_battery_status, struct battery_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_peripheral_battery_state_changed);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
#endif /* !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) */
#endif /* IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY) */

int zmk_widget_dongle_battery_status_init(struct zmk_widget_dongle_battery_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(widget->obj);
    lv_obj_set_size(widget->obj, ROW_BAR_X + ROW_BAR_W + 2 + ROW_PCT_W,
                    BATTERY_SOURCES * ROW_PITCH);

    for (int i = 0; i < BATTERY_SOURCES; i++) {
        lv_coord_t y = i * ROW_PITCH;

        lv_obj_t *name = lv_label_create(widget->obj);
        lv_label_set_text(name, source_name(i));
        lv_obj_set_pos(name, 0, y + (ROW_BAR_H - 8) / 2);

        lv_obj_t *bar = lv_obj_create(widget->obj);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, ROW_BAR_W, ROW_BAR_H);
        lv_obj_set_pos(bar, ROW_BAR_X, y);
        lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(bar, 1, 0);
        lv_obj_set_style_border_color(bar, lv_color_white(), 0);

        lv_obj_t *fill = lv_obj_create(bar);
        lv_obj_remove_style_all(fill);
        lv_obj_set_size(fill, 0, ROW_BAR_H - 4);
        lv_obj_set_pos(fill, 2, 2);
        lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(fill, lv_color_white(), 0);

        lv_obj_t *label = lv_label_create(widget->obj);
        lv_obj_set_width(label, ROW_PCT_W);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(label, ROW_BAR_X + ROW_BAR_W + 2, y + (ROW_BAR_H - 8) / 2);
        lv_label_set_text(label, "");

        lv_obj_add_flag(name, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);

        battery_objects[i] = (struct battery_object){
            .name = name,
            .bar = bar,
            .fill = fill,
            .label = label,
        };
    }

    sys_slist_append(&widgets, &widget->node);

    widget_dongle_battery_status_init();

    return 0;
}

lv_obj_t *zmk_widget_dongle_battery_status_obj(struct zmk_widget_dongle_battery_status *widget) {
    return widget->obj;
}
