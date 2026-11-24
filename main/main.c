#include "app_wifi.h"
#include "bsp_display.h"
#include "darts_engine.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ui_theme.h"
#include "ui_view.h"
#include "app_console.h"
#include "wled_espnow.h"
#include "app_settings.h"
#include "game_store.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char *TAG = "main";

// Firmware counts as healthy once it has run this long. After an OTA update, a
// reset before then makes the bootloader roll back to the previous image.
#define HEALTHY_AFTER_MS 10000

// Consecutive boots that reset before HEALTHY_AFTER_MS. At this many, the next
// boot skips Wi-Fi/ESP-NOW (safe mode) so the serial console stays reachable
// even if a radio setting or a weak power supply is causing the resets.
#define SAFE_MODE_AFTER_FAILED_BOOTS 3

static darts_game_state_t g_game_state;

static uint8_t boot_fail_count_get(void) {
  uint8_t count = 0;
  nvs_handle_t h;
  if (nvs_open("boot", NVS_READONLY, &h) == ESP_OK) {
    nvs_get_u8(h, "fails", &count);
    nvs_close(h);
  }
  return count;
}

static void boot_fail_count_set(uint8_t count) {
  nvs_handle_t h;
  if (nvs_open("boot", NVS_READWRITE, &h) == ESP_OK) {
    nvs_set_u8(h, "fails", count);
    nvs_commit(h);
    nvs_close(h);
  }
}

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

  // Count this boot as failed until it survives HEALTHY_AFTER_MS
  uint8_t failed_boots = boot_fail_count_get();
  bool safe_mode = failed_boots >= SAFE_MODE_AFTER_FAILED_BOOTS;
  boot_fail_count_set(safe_mode ? 0 : failed_boots + 1);
  if (safe_mode) {
    ESP_LOGW(TAG, "SAFE MODE: %u boots in a row reset early. Wi-Fi and ESP-NOW "
                  "are off and the saved game is not restored for this boot; use "
                  "the serial console (e.g. 'wifi forget', 'wled disable') to fix "
                  "the cause, then reboot.",
             failed_boots);
  }

  // Restore the game in progress (survives reboots/power cuts), else new 301.
  // Skipped in safe mode in case the saved game itself is what crashes boot;
  // it stays in NVS until the first turn of the new game overwrites it.
  if (safe_mode || !game_store_load(&g_game_state)) {
    darts_engine_init(&g_game_state, DEFAULT_START_SCORE);
  }

  // Initialize Display Hardware & LVGL first (Priority memory allocation)
  ESP_ERROR_CHECK(bsp_display_init());
  app_settings_load(); // screen timeout + brightness

  // Build UI View under LVGL Lock (Defaulting to Dark Theme)
  bsp_display_lock();
  ui_theme_init(UI_THEME_DARK);
  ui_view_init(&g_game_state);
  bsp_display_unlock();

  // Wi-Fi + OTA web server. Only connects if credentials were saved with the
  // 'wifi' serial command. (Previously disabled for tearing; the display now
  // double-buffers and runs code from PSRAM, see bsp_display.c.)
  if (!safe_mode) {
    app_wifi_init();

    // WLED strip over ESP-NOW (radio only starts if enabled via 'wled enable')
    wled_espnow_init();
    wled_espnow_set_power(true); // Display is on at boot
  }

  // Serial console for configuration (type 'help' or 'wled help')
  app_console_start();

  ESP_LOGI(TAG, "Darts Scoreboard Application running!");

  vTaskDelay(pdMS_TO_TICKS(HEALTHY_AFTER_MS));
  boot_fail_count_set(0);

  // Confirm a freshly OTA-updated image now that it has run stably
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK &&
      ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
      ESP_LOGI(TAG, "New firmware marked valid; rollback cancelled");
    } else {
      ESP_LOGE(TAG, "Failed to mark firmware valid: %s", esp_err_to_name(err));
    }
  }
}
