#include "ui_theme.h"

static const ui_theme_colors_t DARK_THEME = {
    .bg          = LV_COLOR_MAKE(18, 18, 22),     // Deep Slate Dark #121216
    .card_bg     = LV_COLOR_MAKE(30, 30, 38),     // Card Surface #1E1E26
    .card_shadow = LV_COLOR_MAKE(10, 10, 14),     // Subtle Soft Dark Shadow
    .text_main   = LV_COLOR_MAKE(245, 245, 250), // Crisp High Contrast White #F5F5FA
    .text_muted  = LV_COLOR_MAKE(150, 150, 165), // Muted Silver #9696A5
    .primary     = LV_COLOR_MAKE(59, 130, 246),  // Electric Blue #3B82F6
    .submit      = LV_COLOR_MAKE(16, 185, 129),  // Emerald Green #10B981
    .clear       = LV_COLOR_MAKE(239, 68, 68),   // Crimson Red #EF4444
    .bust        = LV_COLOR_MAKE(244, 63, 94),   // Rose Red #F43F5E
    .divider     = LV_COLOR_MAKE(42, 42, 54),    // Border Divider #2A2A36
    .btn_bg      = LV_COLOR_MAKE(38, 38, 48),    // Button Background #262630
    .btn_pressed = LV_COLOR_MAKE(60, 60, 75),    // Pressed State #3C3C4B
};

static const ui_theme_colors_t LIGHT_THEME = {
    .bg          = LV_COLOR_MAKE(240, 240, 240), // Light Background #F0F0F0
    .card_bg     = LV_COLOR_MAKE(255, 255, 255), // Card Surface #FFFFFF
    .card_shadow = LV_COLOR_MAKE(200, 200, 200), // Card Shadow #C8C8C8
    .text_main   = LV_COLOR_MAKE(30, 30, 30),    // Dark Text #1E1E1E
    .text_muted  = LV_COLOR_MAKE(100, 100, 100), // Muted Text #646464
    .primary     = LV_COLOR_MAKE(33, 150, 243),  // Primary Blue #2196F3
    .submit      = LV_COLOR_MAKE(46, 125, 50),   // Green #2E7D32
    .clear       = LV_COLOR_MAKE(198, 40, 40),   // Red #C62828
    .bust        = LV_COLOR_MAKE(211, 47, 47),   // Red #D32F2F
    .divider     = LV_COLOR_MAKE(220, 220, 220), // Divider #DCDCDC
    .btn_bg      = LV_COLOR_MAKE(230, 230, 230), // Button Background #E6E6E6
    .btn_pressed = LV_COLOR_MAKE(200, 200, 200), // Pressed State #C8C8C8
};

static ui_theme_mode_t s_current_mode = UI_THEME_DARK;
const ui_theme_colors_t *g_ui_theme = &DARK_THEME;

void ui_theme_init(ui_theme_mode_t default_mode) {
    ui_theme_set_mode(default_mode);
}

void ui_theme_set_mode(ui_theme_mode_t mode) {
    s_current_mode = mode;
    if (mode == UI_THEME_LIGHT) {
        g_ui_theme = &LIGHT_THEME;
    } else {
        g_ui_theme = &DARK_THEME;
    }
}

ui_theme_mode_t ui_theme_get_mode(void) {
    return s_current_mode;
}
