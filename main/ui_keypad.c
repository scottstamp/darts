#include "ui_keypad.h"
#include "ui_theme.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    ui_keypad_callbacks_t callbacks;
} keypad_ctx_t;

static const char *keypad_map[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    "CLEAR", "0", "ENTER", ""
};

static void btnm_event_cb(lv_event_t *e)
{
    lv_obj_t *btnm = lv_event_get_target(e);
    keypad_ctx_t *ctx = (keypad_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;

    uint32_t btn_id = lv_buttonmatrix_get_selected_button(btnm);
    if (btn_id == LV_BUTTONMATRIX_BUTTON_NONE) return;

    const char *txt = lv_buttonmatrix_get_button_text(btnm, btn_id);
    if (!txt) return;

    if (strcmp(txt, "CLEAR") == 0) {
        if (ctx->callbacks.on_clear) {
            ctx->callbacks.on_clear();
        }
    } else if (strcmp(txt, "ENTER") == 0) {
        if (ctx->callbacks.on_submit) {
            ctx->callbacks.on_submit();
        }
    } else {
        uint8_t digit = (uint8_t)atoi(txt);
        if (ctx->callbacks.on_digit) {
            ctx->callbacks.on_digit(digit);
        }
    }
}

lv_obj_t *ui_keypad_create(lv_obj_t *parent, ui_keypad_callbacks_t callbacks)
{
    keypad_ctx_t *ctx = lv_malloc(sizeof(keypad_ctx_t));
    if (!ctx) return NULL;
    ctx->callbacks = callbacks;

    lv_obj_t *btnm = lv_buttonmatrix_create(parent);

    // Strip default theme styles, transitions, and focus highlights
    lv_obj_remove_style_all(btnm);
    lv_obj_remove_flag(btnm, LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_buttonmatrix_set_map(btnm, keypad_map);
    lv_obj_set_size(btnm, lv_pct(100), lv_pct(100));

    // Trigger button events immediately on touch down, disable popovers
    lv_buttonmatrix_set_button_ctrl_all(btnm, LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_clear_button_ctrl_all(btnm, LV_BUTTONMATRIX_CTRL_POPOVER);

    // Flat styling for container
    lv_obj_set_style_bg_color(btnm, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(btnm, 8, 0);
    lv_obj_set_style_pad_gap(btnm, 8, 0);

    // Flat styling for button items (solid opacity, no shadows or outlines)
    lv_obj_set_style_bg_color(btnm, UI_COLOR_BTN_BG, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_font(btnm, UI_FONT_LARGE, LV_PART_ITEMS);
    lv_obj_set_style_text_color(btnm, UI_COLOR_TEXT_MAIN, LV_PART_ITEMS);
    lv_obj_set_style_radius(btnm, 8, LV_PART_ITEMS);

    // Event handler for button clicks
    lv_obj_add_event_cb(btnm, btnm_event_cb, LV_EVENT_VALUE_CHANGED, ctx);

    return btnm;
}
