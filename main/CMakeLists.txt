idf_component_register(
    SRCS "main.c" "bsp_display.c" "darts_engine.c" "ui_theme.c" "ui_keypad.c" "ui_view.c" "ui_font_segoe_140.c" "ui_font_segoe_54.c" "ui_font_segoe_36.c" "ui_font_segoe_24.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd lvgl esp_lcd_touch_gt911 driver esp_timer nvs_flash
)
