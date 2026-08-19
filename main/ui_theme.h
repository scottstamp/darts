#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_THEME_DARK = 0,
    UI_THEME_LIGHT = 1
} ui_theme_mode_t;

typedef struct {
    lv_color_t bg;
    lv_color_t card_bg;
    lv_color_t card_shadow;
    lv_color_t text_main;
    lv_color_t text_muted;
    lv_color_t primary;
    lv_color_t submit;
    lv_color_t clear;
    lv_color_t bust;
    lv_color_t divider;
    lv_color_t btn_bg;
    lv_color_t btn_pressed;
    lv_color_t out_row_even;
    lv_color_t out_row_odd;
} ui_theme_colors_t;

extern const ui_theme_colors_t *g_ui_theme;

void ui_theme_init(ui_theme_mode_t default_mode);
void ui_theme_set_mode(ui_theme_mode_t mode);
ui_theme_mode_t ui_theme_get_mode(void);

// Theme Colors Dynamic Mappings
#define UI_COLOR_BG            (g_ui_theme->bg)
#define UI_COLOR_CARD_BG       (g_ui_theme->card_bg)
#define UI_COLOR_CARD_SHADOW   (g_ui_theme->card_shadow)
#define UI_COLOR_TEXT_MAIN     (g_ui_theme->text_main)
#define UI_COLOR_TEXT_MUTED    (g_ui_theme->text_muted)
#define UI_COLOR_PRIMARY       (g_ui_theme->primary)
#define UI_COLOR_SUBMIT        (g_ui_theme->submit)
#define UI_COLOR_CLEAR         (g_ui_theme->clear)
#define UI_COLOR_BUST          (g_ui_theme->bust)
#define UI_COLOR_DIVIDER       (g_ui_theme->divider)
#define UI_COLOR_BTN_BG        (g_ui_theme->btn_bg)
#define UI_COLOR_BTN_PRESSED   (g_ui_theme->btn_pressed)
#define UI_COLOR_OUT_ROW_EVEN  (g_ui_theme->out_row_even)
#define UI_COLOR_OUT_ROW_ODD   (g_ui_theme->out_row_odd)

// Segoe UI Custom Fonts
extern const lv_font_t ui_font_segoe_180;
extern const lv_font_t ui_font_segoe_170;
extern const lv_font_t ui_font_segoe_160;
extern const lv_font_t ui_font_segoe_54;
extern const lv_font_t ui_font_segoe_40;
extern const lv_font_t ui_font_segoe_36;
extern const lv_font_t ui_font_segoe_24;

// Font Mappings (Optimized for 0 Flash SPI bus contention)
#define UI_FONT_SCORE          &ui_font_segoe_170
#define UI_FONT_INPUT          &lv_font_montserrat_48
#define UI_FONT_LARGE          &lv_font_montserrat_48
#define UI_FONT_OUT            &lv_font_montserrat_32
#define UI_FONT_MEDIUM         &lv_font_montserrat_24
#define UI_FONT_SMALL          &lv_font_montserrat_18

#ifdef __cplusplus
}
#endif

#endif // UI_THEME_H
