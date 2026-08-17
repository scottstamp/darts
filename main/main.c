#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "bsp_display.h"
#include "darts_engine.h"
#include "ui_view.h"

static const char *TAG = "main";

static darts_game_state_t g_game_state;

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Darts Scoreboard Application...");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Darts Game Engine (301 rule)
    darts_engine_init(&g_game_state, 301);

    // Initialize Display Hardware & LVGL
    ESP_ERROR_CHECK(bsp_display_init());

    // Build UI View under LVGL Lock (Defaulting to Dark Theme)
    bsp_display_lock();
    ui_theme_init(UI_THEME_DARK);
    ui_view_init(&g_game_state);
    bsp_display_unlock();

    ESP_LOGI(TAG, "Darts Scoreboard Application running!");
}
