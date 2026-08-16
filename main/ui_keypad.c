#include "ui_keypad.h"
#include "ui_theme.h"

typedef struct {
    ui_keypad_callbacks_t callbacks;
} keypad_ctx_t;

static void btn_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_current_target(e);
    keypad_ctx_t *ctx = (keypad_ctx_t *)lv_event_get_user_data(e);
    uintptr_t btn_id = (uintptr_t)lv_obj_get_user_data(btn);

    if (!ctx) return;

    if (btn_id <= 9) {
        if (ctx->callbacks.on_digit) {
            ctx->callbacks.on_digit((uint8_t)btn_id);
        }
    } else if (btn_id == 10) { // CLEAR
        if (ctx->callbacks.on_clear) {
            ctx->callbacks.on_clear();
        }
    } else if (btn_id == 11) { // SUBMIT
        if (ctx->callbacks.on_submit) {
            ctx->callbacks.on_submit();
        }
    }
}

lv_obj_t *ui_keypad_create(lv_obj_t *parent, ui_keypad_callbacks_t callbacks)
{
    keypad_ctx_t *ctx = lv_malloc(sizeof(keypad_ctx_t));
    if (!ctx) return NULL;
    ctx->callbacks = callbacks;

    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(container, UI_COLOR_BG, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 10, 0);

    // Layout configuration
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    lv_obj_set_layout(container, LV_LAYOUT_GRID);
    lv_obj_set_style_grid_column_dsc_array(container, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(container, row_dsc, 0);
    lv_obj_set_style_pad_column(container, 8, 0);
    lv_obj_set_style_pad_row(container, 8, 0);

    struct {
        const char *label;
        uintptr_t id;
        uint8_t col;
        uint8_t row;
        lv_color_t bg_color;
        const lv_font_t *font;
    } buttons[] = {
        {"1", 1, 0, 0, UI_COLOR_BTN_BG, UI_FONT_LARGE},
        {"2", 2, 1, 0, UI_COLOR_BTN_BG, UI_FONT_LARGE},
        {"3", 3, 2, 0, UI_COLOR_BTN_BG, UI_FONT_LARGE},
        {"4", 4, 0, 1, UI_COLOR_BTN_BG, UI_FONT_LARGE},
        {"5", 5, 1, 1, UI_COLOR_BTN_BG, UI_FONT_LARGE},
        {"6", 6, 2, 1, UI_COLOR_BTN_BG, UI_FONT_LARGE},
        {"7", 7, 0, 2, UI_COLOR_BTN_BG, UI_FONT_LARGE},
        {"8", 8, 1, 2, UI_COLOR_BTN_BG, UI_FONT_LARGE},
        {"9", 9, 2, 2, UI_COLOR_BTN_BG, UI_FONT_LARGE},
        {"CLEAR", 10, 0, 3, UI_COLOR_CLEAR, UI_FONT_SMALL},
        {"0", 0, 1, 3, UI_COLOR_BTN_BG, UI_FONT_LARGE},
        {"ENTER", 11, 2, 3, UI_COLOR_SUBMIT, UI_FONT_SMALL},
    };

    size_t num_buttons = sizeof(buttons) / sizeof(buttons[0]);

    for (size_t i = 0; i < num_buttons; i++) {
        lv_obj_t *btn = lv_btn_create(container);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, buttons[i].col, 1, LV_GRID_ALIGN_STRETCH, buttons[i].row, 1);
        lv_obj_set_user_data(btn, (void *)buttons[i].id);

        // Styling
        lv_obj_set_style_bg_color(btn, buttons[i].bg_color, 0);
        lv_obj_set_style_bg_color(btn, UI_COLOR_BTN_PRESSED, LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_shadow_width(btn, 2, 0);
        lv_obj_set_style_shadow_color(btn, lv_color_make(180, 180, 180), 0);

        lv_obj_t *label = lv_label_create(btn);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
        lv_label_set_text(label, buttons[i].label);
        lv_obj_set_style_text_font(label, buttons[i].font, 0);

        if (buttons[i].id == 10 || buttons[i].id == 11) {
            lv_obj_set_style_text_color(label, lv_color_white(), 0);
        } else {
            lv_obj_set_style_text_color(label, UI_COLOR_TEXT_MAIN, 0);
        }

        lv_obj_center(label);
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, ctx);
    }

    return container;
}
