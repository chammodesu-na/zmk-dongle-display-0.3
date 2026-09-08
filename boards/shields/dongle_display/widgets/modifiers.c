/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/modifiers.h>

#include "modifiers.h"

struct modifiers_state {    
    uint8_t modifiers;
};

struct modifier_symbol {    
    uint8_t modifier;
    const lv_img_dsc_t *symbol_dsc;
    lv_obj_t *symbol;
    lv_obj_t *highlight;
    bool is_active;
};

LV_IMG_DECLARE(control_icon);
struct modifier_symbol ms_control = {
    .modifier = MOD_LCTL | MOD_RCTL,
    .symbol_dsc = &control_icon,
};

LV_IMG_DECLARE(shift_icon);
struct modifier_symbol ms_shift = {
    .modifier = MOD_LSFT | MOD_RSFT,
    .symbol_dsc = &shift_icon,
};

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MAC_MODIFIERS)
LV_IMG_DECLARE(opt_icon);
struct modifier_symbol ms_opt = {
    .modifier = MOD_LALT | MOD_RALT,
    .symbol_dsc = &opt_icon,
};

LV_IMG_DECLARE(cmd_icon);
struct modifier_symbol ms_cmd = {
    .modifier = MOD_LGUI | MOD_RGUI,
    .symbol_dsc = &cmd_icon,
};

struct modifier_symbol *modifier_symbols[] = {
    // this order determines the order of the symbols
    &ms_control,
    &ms_opt,
    &ms_cmd,
    &ms_shift
};
#else
LV_IMG_DECLARE(alt_icon);
struct modifier_symbol ms_alt = {
    .modifier = MOD_LALT | MOD_RALT,
    .symbol_dsc = &alt_icon,
};

LV_IMG_DECLARE(win_icon);
struct modifier_symbol ms_win = {
    .modifier = MOD_LGUI | MOD_RGUI,
    .symbol_dsc = &win_icon,
};

struct modifier_symbol *modifier_symbols[] = {
    // this order determines the order of the symbols
    &ms_win,
    &ms_alt,
    &ms_control,
    &ms_shift
};
#endif

#define NUM_SYMBOLS (sizeof(modifier_symbols) / sizeof(struct modifier_symbol *))

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* An active modifier gets a bright frame drawn around its icon. On a 1-bit
 * OLED that reads much faster than the thin underline it replaces.
 */
static void set_modifiers(lv_obj_t *widget, struct modifiers_state state) {
    for (int i = 0; i < NUM_SYMBOLS; i++) {
        bool mod_is_active = state.modifiers & modifier_symbols[i]->modifier;

        if (mod_is_active == modifier_symbols[i]->is_active) {
            continue;
        }

        lv_obj_set_style_border_opa(modifier_symbols[i]->highlight,
                                    mod_is_active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        modifier_symbols[i]->is_active = mod_is_active;
    }
}

void modifiers_update_cb(struct modifiers_state state) {
    struct zmk_widget_modifiers *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_modifiers(widget->obj, state); }
}

static struct modifiers_state modifiers_get_state(const zmk_event_t *eh) {
    return (struct modifiers_state) {
        .modifiers = zmk_hid_get_explicit_mods()
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_modifiers, struct modifiers_state,
                            modifiers_update_cb, modifiers_get_state)

ZMK_SUBSCRIPTION(widget_modifiers, zmk_keycode_state_changed);

int zmk_widget_modifiers_init(struct zmk_widget_modifiers *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(widget->obj);
    lv_obj_set_size(widget->obj, NUM_SYMBOLS * MODIFIER_CELL_SIZE, MODIFIER_CELL_SIZE);

    for (int i = 0; i < NUM_SYMBOLS; i++) {
        lv_coord_t x = i * MODIFIER_CELL_SIZE;

        modifier_symbols[i]->highlight = lv_obj_create(widget->obj);
        lv_obj_remove_style_all(modifier_symbols[i]->highlight);
        lv_obj_set_size(modifier_symbols[i]->highlight, MODIFIER_CELL_SIZE, MODIFIER_CELL_SIZE);
        lv_obj_set_pos(modifier_symbols[i]->highlight, x, 0);
        lv_obj_set_style_bg_opa(modifier_symbols[i]->highlight, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(modifier_symbols[i]->highlight, 2, 0);
        lv_obj_set_style_border_width(modifier_symbols[i]->highlight, 2, 0);
        lv_obj_set_style_border_color(modifier_symbols[i]->highlight, lv_color_white(), 0);
        lv_obj_set_style_border_opa(modifier_symbols[i]->highlight, LV_OPA_TRANSP, 0);

        modifier_symbols[i]->symbol = lv_img_create(widget->obj);
        lv_obj_set_pos(modifier_symbols[i]->symbol, x + (MODIFIER_CELL_SIZE - SIZE_SYMBOLS) / 2,
                       (MODIFIER_CELL_SIZE - SIZE_SYMBOLS) / 2);
        lv_img_set_src(modifier_symbols[i]->symbol, modifier_symbols[i]->symbol_dsc);

        modifier_symbols[i]->is_active = false;
    }

    sys_slist_append(&widgets, &widget->node);

    widget_modifiers_init();

    return 0;
}

lv_obj_t *zmk_widget_modifiers_obj(struct zmk_widget_modifiers *widget) {
    return widget->obj;
}