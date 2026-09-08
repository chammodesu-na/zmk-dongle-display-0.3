/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
 
 #pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

#define SIZE_SYMBOLS 14 // 14 x 14 pixel
#define MODIFIER_CELL_SIZE 18 // icon plus the active-state frame around it

struct zmk_widget_modifiers {
    sys_snode_t node;
    lv_obj_t *obj;
};

int zmk_widget_modifiers_init(struct zmk_widget_modifiers *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_modifiers_obj(struct zmk_widget_modifiers *widget);