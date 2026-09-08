/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
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

/* All battery levels share a single line of the 8x16 font, which is the
 * largest crisp bitmap font available:
 *
 *   L 87%  R 41%
 *
 * That is 16 characters at most across a 128px panel. Once a third battery
 * or a three-digit level no longer fits, the percent signs are dropped
 * rather than shrinking the text.
 */
#define BATTERY_LINE_CHARS 16
/* Wide enough for every source at "X 100%" plus separators, so the line is
 * never truncated and its true length can be measured. */
#define BATTERY_TEXT_LEN   (BATTERY_SOURCES * 8 + 1)

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct battery_state {
    uint8_t source;
    uint8_t level;
    bool usb_present;
};

/* Levels arrive one event at a time, so the last value of every source is
 * kept to be able to redraw the whole line. */
static struct {
    uint8_t level;
    bool usb_present;
    bool reported;
} battery_levels[BATTERY_SOURCES];

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

/* Writes the line into buf (sized BATTERY_TEXT_LEN) and returns its length in
 * characters, so the caller can retry with a narrower format when it does not
 * fit the panel. */
static size_t format_levels(char *buf, bool with_percent) {
    size_t used = 0;

    buf[0] = '\0';

    for (int i = 0; i < BATTERY_SOURCES; i++) {
        if (!battery_levels[i].reported) {
            continue;
        }

        char value[6];

        if (battery_levels[i].usb_present) {
            strcpy(value, "CHG");
        } else {
            snprintf(value, sizeof(value), "%d%s", battery_levels[i].level,
                     with_percent ? "%" : "");
        }

        used += snprintf(buf + used, BATTERY_TEXT_LEN - used, "%s%s %s",
                         (used > 0) ? "  " : "", source_name(i), value);
    }

    return used;
}

static void set_battery_symbol(lv_obj_t *label, struct battery_state state) {
    if (state.source >= BATTERY_SOURCES) {
        return;
    }
    LOG_DBG("source: %d, level: %d, usb: %d", state.source, state.level, state.usb_present);

    battery_levels[state.source].level = state.level > 100 ? 100 : state.level;
    battery_levels[state.source].usb_present = state.usb_present;

    /* A source that has never reported anything stays off the line instead of
     * claiming 0%. */
    battery_levels[state.source].reported = (state.level > 0 || state.usb_present);

    char text[BATTERY_TEXT_LEN];

    if (format_levels(text, true) > BATTERY_LINE_CHARS) {
        format_levels(text, false);
    }

    lv_label_set_text(label, text);
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
    widget->obj = lv_label_create(parent);

    lv_obj_set_width(widget->obj, 128);
    lv_obj_set_style_text_font(widget->obj, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_letter_space(widget->obj, 0, 0);
    lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(widget->obj, "");

    sys_slist_append(&widgets, &widget->node);

    widget_dongle_battery_status_init();

    return 0;
}

lv_obj_t *zmk_widget_dongle_battery_status_obj(struct zmk_widget_dongle_battery_status *widget) {
    return widget->obj;
}
