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
 *   | BT1 (*)                        L 87% |  0..15  endpoint + batteries
 *   |                                R 41% |
 *   |--------------------------------------|         divider
 *   |                QWERTY                |         active layer, centred
 *   |                                      |
 *   | [#][A][^][^]                    CLCK |  46..63 modifiers + lock state
 *   +--------------------------------------+
 *
 * The top band grows with the number of batteries, so the divider and the
 * layer name are placed from its measured height rather than a fixed offset.
 * On a 128x32 panel the layer band is what gets clipped first.
 */
#define SCREEN_H       64
#define TOP_BAND_MIN_H 16
#define LAYER_H        16
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

    int top_band_h = TOP_BAND_MIN_H;

    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_LEFT, 0, 0);

#if IS_ENABLED(CONFIG_ZMK_BATTERY)
    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, screen);
    lv_obj_align(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget),
                 LV_ALIGN_TOP_RIGHT, 0, 0);

    int battery_h = zmk_widget_dongle_battery_status_height();
    if (battery_h > top_band_h) {
        top_band_h = battery_h;
    }
#endif

    lv_obj_t *divider = lv_obj_create(screen);
    lv_obj_remove_style_all(divider);
    lv_obj_set_size(divider, 128, 1);
    lv_obj_align(divider, LV_ALIGN_TOP_LEFT, 0, top_band_h + 1);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(divider, lv_color_white(), 0);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
    /* The layer name gets the whole middle band to itself, in the large font.
     * Its width and alignment stay under the control of the Kconfig options. */
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_t *layer_obj = zmk_widget_layer_status_obj(&layer_status_widget);
    lv_obj_set_style_text_font(layer_obj, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_letter_space(layer_obj, 0, 0);

    int band_top = top_band_h + 2;
    int band_bottom = SCREEN_H - BOTTOM_BAND_H;
    lv_obj_align(layer_obj, LV_ALIGN_TOP_MID, 0,
                 band_top + (band_bottom - band_top - LAYER_H) / 2);
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
