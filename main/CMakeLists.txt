idf_component_register(
    SRCS "ui_font_segoe_170.c" "main.c" "bsp_display.c" "darts_engine.c" "ui_theme.c" "ui_keypad.c" "ui_view.c" "app_wifi.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd lvgl esp_lcd_touch_gt911 driver esp_timer nvs_flash esp_wifi esp_event esp_http_server app_update
)

# Disable GCC optimization passes on static font data files for 10x faster compilation
set_source_files_properties(
    "ui_font_segoe_170.c"
    PROPERTIES COMPILE_FLAGS "-O0"
)
