#include "ui_theme.h"

static const ui_theme_colors_t DARK_THEME = {
    .bg           = LV_COLOR_MAKE(0, 0, 0),        // Pure Black #000000
    .card_bg      = LV_COLOR_MAKE(0, 0, 0),        // Pure Black #000000
    .card_shadow  = LV_COLOR_MAKE(0, 0, 0),        // No Shadow
    .text_main    = LV_COLOR_MAKE(255, 255, 255), // Pure White #FFFFFF
    .text_muted   = LV_COLOR_MAKE(160, 160, 175), // Muted Silver #A0A0AF
    .primary      = LV_COLOR_MAKE(59, 130, 246),  // Electric Blue #3B82F6
    .submit       = LV_COLOR_MAKE(34, 110, 64),   // Muted Green #226E40
    .clear        = LV_COLOR_MAKE(140, 40, 40),   // Muted Red #8C2828
    .bust         = LV_COLOR_MAKE(244, 63, 94),   // Rose Red #F43F5E
    .divider      = LV_COLOR_MAKE(40, 40, 50),    // Border Divider #282832
    .btn_bg       = LV_COLOR_MAKE(30, 30, 38),    // Button Background #1E1E26
    .btn_pressed  = LV_COLOR_MAKE(50, 50, 64),    // Pressed State
    .out_row_even = LV_COLOR_MAKE(18, 18, 24),    // Alternating Out Row (Even)
    .out_row_odd  = LV_COLOR_MAKE(32, 32, 42),    // Alternating Out Row (Odd)
};

static const ui_theme_colors_t LIGHT_THEME = {
    .bg           = LV_COLOR_MAKE(240, 240, 240), // Light Background #F0F0F0
    .card_bg      = LV_COLOR_MAKE(255, 255, 255), // Card Surface #FFFFFF
    .card_shadow  = LV_COLOR_MAKE(200, 200, 200), // Card Shadow #C8C8C8
    .text_main    = LV_COLOR_MAKE(30, 30, 30),    // Dark Text #1E1E1E
    .text_muted   = LV_COLOR_MAKE(100, 100, 100), // Muted Text #646464
    .primary      = LV_COLOR_MAKE(33, 150, 243),  // Primary Blue #2196F3
    .submit       = LV_COLOR_MAKE(50, 150, 90),   // Muted Green #32965A
    .clear        = LV_COLOR_MAKE(200, 75, 75),   // Muted Red #C84B4B
    .bust         = LV_COLOR_MAKE(211, 47, 47),   // Red #D32F2F
    .divider      = LV_COLOR_MAKE(220, 220, 220), // Divider #DCDCDC
    .btn_bg       = LV_COLOR_MAKE(230, 230, 230), // Button Background #E6E6E6
    .btn_pressed  = LV_COLOR_MAKE(200, 200, 200), // Pressed State #C8C8C8
    .out_row_even = LV_COLOR_MAKE(250, 250, 250), // Alternating Out Row (Even)
    .out_row_odd  = LV_COLOR_MAKE(235, 235, 240), // Alternating Out Row (Odd)
};

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *THEME_TAG = "ui_theme";

static ui_theme_mode_t s_current_mode = UI_THEME_DARK;
const ui_theme_colors_t *g_ui_theme = &DARK_THEME;

void ui_theme_init(ui_theme_mode_t default_mode) {
    nvs_handle_t handle;
    ui_theme_mode_t active_mode = default_mode;
    if (nvs_open("app_settings", NVS_READONLY, &handle) == ESP_OK) {
        uint8_t saved_mode = 0;
        if (nvs_get_u8(handle, "theme_mode", &saved_mode) == ESP_OK) {
            active_mode = (ui_theme_mode_t)saved_mode;
            ESP_LOGI(THEME_TAG, "Restored theme mode from NVS: %d", active_mode);
        }
        nvs_close(handle);
    }
    ui_theme_set_mode(active_mode);
}

void ui_theme_set_mode(ui_theme_mode_t mode) {
    s_current_mode = mode;
    if (mode == UI_THEME_LIGHT) {
        g_ui_theme = &LIGHT_THEME;
    } else {
        g_ui_theme = &DARK_THEME;
    }

    nvs_handle_t handle;
    if (nvs_open("app_settings", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, "theme_mode", (uint8_t)mode);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(THEME_TAG, "Saved theme mode to NVS: %d", mode);
    }
}

ui_theme_mode_t ui_theme_get_mode(void) {
    return s_current_mode;
}
