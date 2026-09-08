/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"
#include "widgets/battery_status.h"
#include "widgets/modifiers.h"
#include "widgets/layer_status.h"
#include "widgets/output_status.h"
#include "widgets/hid_indicators.h"
#include "widgets/wpm_status.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* Layout for a 128x64 panel, three bands separated by a rule:
 *
 *   +--------------------------------------+
 *   | BT1 (*)                       QWERTY |  0..15  endpoint + active layer
 *   |--------------------------------------|  16     divider
 *   |          L 87%  R 41%                |  23..38 battery levels
 *   |                                      |
 *   | [#][A][^][^]                    CLCK |  46..63 modifiers + lock state
 *   +--------------------------------------+
 *
 * On a 128x32 panel the battery band is what gets clipped first.
 */
#define TOP_BAND_H     16
#define DIVIDER_Y      16
#define BATTERY_Y      23
#define BOTTOM_BAND_H  MODIFIER_CELL_SIZE

static struct zmk_widget_output_status output_status_widget;
static struct zmk_widget_dongle_battery_status dongle_battery_status_widget;

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
static struct zmk_widget_layer_status layer_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
static struct zmk_widget_modifiers modifiers_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
static struct zmk_widget_hid_indicators hid_indicators_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
static struct zmk_widget_wpm_status wpm_status_widget;
#endif

lv_style_t global_style;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;

    screen = lv_obj_create(NULL);

    lv_style_init(&global_style);
    lv_style_set_text_font(&global_style, &lv_font_unscii_8);
    lv_style_set_text_letter_space(&global_style, 1);
    lv_style_set_text_line_space(&global_style, 1);
    lv_obj_add_style(screen, &global_style, LV_PART_MAIN);

    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_LEFT, 0, 0);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
    /* The layer name shares the top band with the endpoint, right aligned and
     * in the large font so the two most-glanced-at values sit on one line. */
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_t *layer_obj = zmk_widget_layer_status_obj(&layer_status_widget);
    lv_obj_set_style_text_font(layer_obj, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_letter_space(layer_obj, 0, 0);
    lv_obj_set_style_text_align(layer_obj, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(layer_obj, LV_ALIGN_TOP_RIGHT, 0, 0);
#endif

    lv_obj_t *divider = lv_obj_create(screen);
    lv_obj_remove_style_all(divider);
    lv_obj_set_size(divider, 128, 1);
    lv_obj_align(divider, LV_ALIGN_TOP_LEFT, 0, DIVIDER_Y);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(divider, lv_color_white(), 0);

#if IS_ENABLED(CONFIG_ZMK_BATTERY)
    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, screen);
    lv_obj_align(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget),
                 LV_ALIGN_TOP_MID, 0, BATTERY_Y);
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
    zmk_widget_modifiers_init(&modifiers_widget, screen);
    lv_obj_align(zmk_widget_modifiers_obj(&modifiers_widget), LV_ALIGN_BOTTOM_LEFT, 0, 0);
#endif

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    /* Locks and WPM stack in the free corner to the right of the modifiers. */
    zmk_widget_hid_indicators_init(&hid_indicators_widget, screen);
    lv_obj_align(zmk_widget_hid_indicators_obj(&hid_indicators_widget), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
    zmk_widget_wpm_status_init(&wpm_status_widget, screen);
    lv_obj_align(zmk_widget_wpm_status_obj(&wpm_status_widget), LV_ALIGN_BOTTOM_RIGHT, 0,
                 -(BOTTOM_BAND_H / 2));
#endif

    return screen;
}
