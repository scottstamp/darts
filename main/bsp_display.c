#include "bsp_display.h"
#include "ui_view.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"

static const char *TAG = "bsp_display";

static esp_lcd_panel_handle_t s_panel_handle = NULL;
static esp_lcd_touch_handle_t s_touch_handle = NULL;
static SemaphoreHandle_t s_lvgl_mutex = NULL;

static bool s_display_is_asleep = false;
static bool s_ignore_current_touch = false;

bool bsp_display_is_asleep(void) {
  return s_display_is_asleep;
}

void bsp_display_wake(void) {
  if (s_display_is_asleep) {
    s_display_is_asleep = false;
    bsp_display_backlight_set(true);
    lv_display_trigger_activity(NULL);
    ESP_LOGI(TAG, "Display woken up manually");
  }
}

static void lvgl_tick_cb(void *arg) { lv_tick_inc(2); }

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area,
                          uint8_t *px_map) {
  esp_lcd_panel_draw_bitmap(s_panel_handle, area->x1, area->y1, area->x2 + 1,
                            area->y2 + 1, px_map);
  lv_display_flush_ready(disp);
  esp_task_wdt_reset();
}

static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  esp_lcd_touch_point_data_t touch_data[1];
  uint8_t touch_cnt = 0;

  esp_lcd_touch_read_data(s_touch_handle);
  esp_err_t err =
      esp_lcd_touch_get_data(s_touch_handle, touch_data, &touch_cnt, 1);

  if (err == ESP_OK && touch_cnt > 0) {
    if (s_display_is_asleep) {
      // Display was asleep: wake screen, perform hard reset + re-init, and suppress click
      s_display_is_asleep = false;
      s_ignore_current_touch = true;

      // Full hardware panel reset and re-initialization
      if (s_panel_handle) {
        esp_lcd_panel_reset(s_panel_handle);
        esp_lcd_panel_init(s_panel_handle);
        esp_lcd_panel_disp_on_off(s_panel_handle, true);
      }

      ui_view_wake();
      lv_obj_invalidate(lv_scr_act());
      bsp_display_backlight_set(true);
      lv_display_trigger_activity(NULL);
      ESP_LOGI(TAG, "Wake-up touch detected: ST7701S panel hard reset & re-initialized, backlight ON.");
    }

    if (s_ignore_current_touch) {
      // Continue ignoring this touch until finger is completely lifted
      data->state = LV_INDEV_STATE_REL;
    } else {
      // Normal UI touch input
      data->point.x = touch_data[0].x;
      data->point.y = touch_data[0].y;
      data->state = LV_INDEV_STATE_PR;
    }
  } else {
    // Finger lifted off screen: reset touch suppression flag
    s_ignore_current_touch = false;
    data->state = LV_INDEV_STATE_REL;
  }
}

static bool s_ota_in_progress = false;

static void lvgl_task(void *arg) {
  ESP_LOGI(TAG, "LVGL task started");
  esp_task_wdt_add(NULL);
  while (1) {
    esp_task_wdt_reset();

    if (s_ota_in_progress) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    bsp_display_lock();

    // Check for screen inactivity timeout
    if (g_screen_timeout_sec > 0 && !s_display_is_asleep) {
      uint32_t inactive_ms = lv_display_get_inactive_time(NULL);
      if (inactive_ms >= (g_screen_timeout_sec * 1000)) {
        s_display_is_asleep = true;
        bsp_display_backlight_set(false);
        if (s_panel_handle) {
          esp_lcd_panel_disp_on_off(s_panel_handle, false);
        }
        ui_view_sleep();
        ESP_LOGI(TAG, "Screen inactive for %lu ms (%lu sec limit). ST7701S panel & backlight powered OFF.",
                 (unsigned long)inactive_ms, (unsigned long)g_screen_timeout_sec);
      }
    }

    uint32_t delay_ms = lv_timer_handler();
    esp_task_wdt_reset();
    bsp_display_unlock();

    if (delay_ms < 16) {
      delay_ms = 16;
    } else if (delay_ms > 50) {
      delay_ms = 50;
    }
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
}

void bsp_display_pause_for_ota(void) {
  s_ota_in_progress = true;
  bsp_display_backlight_set(false);
}

void bsp_display_backlight_set(bool enable) {
  gpio_set_level(BSP_LCD_BK_LIGHT, enable ? 1 : 0);
}

void bsp_display_lock(void) {
  if (s_lvgl_mutex) {
    xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
  }
}

void bsp_display_unlock(void) {
  if (s_lvgl_mutex) {
    xSemaphoreGive(s_lvgl_mutex);
  }
}

esp_err_t bsp_display_init(void) {
  ESP_LOGI(TAG, "Initializing display BSP...");
  s_lvgl_mutex = xSemaphoreCreateMutex();

  // 1. Backlight GPIO setup
  gpio_config_t bk_gpio_config = {.mode = GPIO_MODE_OUTPUT,
                                  .pin_bit_mask = 1ULL << BSP_LCD_BK_LIGHT};
  gpio_config(&bk_gpio_config);
  bsp_display_backlight_set(true);

  // 2. ST7262 RGB Panel Driver Init
  esp_lcd_rgb_panel_config_t rgb_config = {
      .clk_src = LCD_CLK_SRC_PLL160M,
      .dma_burst_size = 64,
      .timings =
          {
              .pclk_hz = 15400000,
              .h_res = BSP_LCD_H_RES,
              .v_res = BSP_LCD_V_RES,
              .hsync_pulse_width = 4,
              .hsync_back_porch = 43,
              .hsync_front_porch = 40,
              .vsync_pulse_width = 4,
              .vsync_back_porch = 12,
              .vsync_front_porch = 20,
              .flags =
                  {
                      .pclk_active_neg = 1,
                  },
          },
      .data_width = 16,
      .bounce_buffer_size_px = BSP_LCD_H_RES * 30,
      .de_gpio_num = BSP_LCD_DE,
      .pclk_gpio_num = BSP_LCD_PCLK,
      .hsync_gpio_num = BSP_LCD_HSYNC,
      .vsync_gpio_num = BSP_LCD_VSYNC,
      .data_gpio_nums =
          {
              BSP_LCD_DATA_R0,
              BSP_LCD_DATA_R1,
              BSP_LCD_DATA_R2,
              BSP_LCD_DATA_R3,
              BSP_LCD_DATA_R4,
              BSP_LCD_DATA_G0,
              BSP_LCD_DATA_G1,
              BSP_LCD_DATA_G2,
              BSP_LCD_DATA_G3,
              BSP_LCD_DATA_G4,
              BSP_LCD_DATA_G5,
              BSP_LCD_DATA_B0,
              BSP_LCD_DATA_B1,
              BSP_LCD_DATA_B2,
              BSP_LCD_DATA_B3,
              BSP_LCD_DATA_B4,
          },
      .disp_gpio_num = GPIO_NUM_NC,
      .flags =
          {
              .fb_in_psram = 1,
              .bb_invalidate_cache = 1,
          },
  };

  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&rgb_config, &s_panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));

  // 3. I2C Master & GT911 Touch Controller Init
  i2c_master_bus_handle_t i2c_bus = NULL;
  i2c_master_bus_config_t i2c_bus_config = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .i2c_port = I2C_NUM_0,
      .sda_io_num = BSP_I2C_SDA,
      .scl_io_num = BSP_I2C_SCL,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = false,
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus));

  esp_lcd_panel_io_handle_t tp_io_handle = NULL;
  esp_lcd_panel_io_i2c_config_t tp_io_config =
      ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io_handle));

  esp_lcd_touch_config_t tp_cfg = {
      .x_max = BSP_LCD_H_RES,
      .y_max = BSP_LCD_V_RES,
      .rst_gpio_num = BSP_TOUCH_RST,
      .int_gpio_num = BSP_TOUCH_INT,
      .levels =
          {
              .reset = 0,
              .interrupt = 0,
          },
      .flags =
          {
              .swap_xy = 0,
              .mirror_x = 0,
              .mirror_y = 0,
          },
  };
  ESP_ERROR_CHECK(
      esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &s_touch_handle));

  // 4. Register Fast Internal SRAM DMA Buffers (10 lines = 16KB per buffer, 0 PSRAM bus contention)
  lv_init();

  size_t buf_size = BSP_LCD_H_RES * 10 * sizeof(lv_color_t);
  void *buf1 =
      heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  void *buf2 =
      heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  assert(buf1 != NULL && buf2 != NULL);

  lv_display_t *disp = lv_display_create(BSP_LCD_H_RES, BSP_LCD_V_RES);
  lv_display_set_flush_cb(disp, lvgl_flush_cb);
  lv_display_set_buffers(disp, buf1, buf2, buf_size,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, lvgl_touch_read_cb);
  lv_timer_t *read_timer = lv_indev_get_read_timer(indev);
  if (read_timer) {
    lv_timer_set_period(read_timer, 10);
  }

  // 5. LVGL Periodic Timer Setup (2ms tick)
  const esp_timer_create_args_t lvgl_tick_timer_args = {
      .callback = &lvgl_tick_cb, .name = "lvgl_tick"};
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 2000));

  // Create FreeRTOS task to run lv_timer_handler()
  xTaskCreatePinnedToCore(lvgl_task, "LVGL Task", 16384, NULL, 5, NULL, 1);

  ESP_LOGI(TAG, "Display BSP successfully initialized!");
  return ESP_OK;
}
