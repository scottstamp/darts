#include "app_wifi.h"
#include "bsp_display.h"
#include "darts_engine.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ui_theme.h"
#include "ui_view.h"
#include "app_console.h"
#include "wled_espnow.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char *TAG = "main";

// Configurable screen timeout in seconds (Default: 5 minutes = 300 seconds)
uint32_t g_screen_timeout_sec = 300;

// Firmware counts as healthy once it has run this long after an OTA update;
// a crash before then makes the bootloader roll back to the previous image.
#define OTA_HEALTHY_AFTER_MS 10000

static darts_game_state_t g_game_state;

void app_main(void) {
  ESP_LOGI(TAG, "Starting Darts Scoreboard Application...");

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Initialize Darts Game Engine (301 rule)
  darts_engine_init(&g_game_state, 301);

  // Initialize Display Hardware & LVGL first (Priority memory allocation)
  ESP_ERROR_CHECK(bsp_display_init());

  // Build UI View under LVGL Lock (Defaulting to Dark Theme)
  bsp_display_lock();
  ui_theme_init(UI_THEME_DARK);
  ui_view_init(&g_game_state);
  bsp_display_unlock();

  // Wi-Fi + OTA web server. Only connects if credentials were saved with the
  // 'wifi' serial command. (Previously disabled for tearing; the display now
  // double-buffers and runs code from PSRAM, see bsp_display.c.)
  app_wifi_init();

  // WLED strip over ESP-NOW (radio only starts if enabled via 'wled enable')
  wled_espnow_init();
  wled_espnow_set_power(true); // Display is on at boot

  // Serial console for configuration (type 'help' or 'wled help')
  app_console_start();

  ESP_LOGI(TAG, "Darts Scoreboard Application running!");

  // Confirm a freshly OTA-updated image only after it has run stably
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK &&
      ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
    vTaskDelay(pdMS_TO_TICKS(OTA_HEALTHY_AFTER_MS));
    ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
    ESP_LOGI(TAG, "New firmware marked valid; rollback cancelled");
  }
}
