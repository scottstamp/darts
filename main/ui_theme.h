#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Theme Colors (Light mode default)
#define UI_COLOR_BG            lv_color_make(240, 240, 240) // #F0F0F0
#define UI_COLOR_CARD_BG       lv_color_make(255, 255, 255) // #FFFFFF
#define UI_COLOR_TEXT_MAIN     lv_color_make(30, 30, 30)    // #1E1E1E
#define UI_COLOR_TEXT_MUTED    lv_color_make(100, 100, 100) // #646464
#define UI_COLOR_PRIMARY       lv_color_make(33, 150, 243)  // #2196F3
#define UI_COLOR_SUBMIT        lv_color_make(46, 125, 50)   // Dark Green #2E7D32
#define UI_COLOR_CLEAR         lv_color_make(198, 40, 40)   // Dark Red #C62828
#define UI_COLOR_BUST          lv_color_make(211, 47, 47)   // Red #D32F2F
#define UI_COLOR_DIVIDER       lv_color_make(220, 220, 220) // #DCDCDC
#define UI_COLOR_BTN_BG        lv_color_make(230, 230, 230) // #E6E6E6
#define UI_COLOR_BTN_PRESSED   lv_color_make(200, 200, 200) // #C8C8C8

// Segoe UI Custom Fonts
extern const lv_font_t ui_font_segoe_140;
extern const lv_font_t ui_font_segoe_36;
extern const lv_font_t ui_font_segoe_24;

// Font Mappings
#define UI_FONT_SCORE          &ui_font_segoe_140
#define UI_FONT_LARGE          &ui_font_segoe_36
#define UI_FONT_MEDIUM         &ui_font_segoe_24
#define UI_FONT_SMALL          &ui_font_segoe_24

#ifdef __cplusplus
}
#endif

#endif // UI_THEME_H
