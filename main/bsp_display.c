#include "bsp_display.h"
#include "ui_view.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
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

// Keep this constant while running: the panel's timing controller drops into
// its built-in W/R/G/B self-test pattern if the clock goes out of spec (e.g.
// slowing it during sleep), and doesn't recover cleanly when it's restored.
#define LCD_PCLK_HZ        15400000
// Longest wait for a frame-buffer swap (one frame is ~30 ms)
#define FRAME_SWAP_TIMEOUT_MS 100

// Backlight PWM. 20 kHz keeps the backlight driver out of the audible range.
#define BACKLIGHT_LEDC_TIMER    LEDC_TIMER_0
#define BACKLIGHT_LEDC_CHANNEL  LEDC_CHANNEL_0
#define BACKLIGHT_LEDC_RES      LEDC_TIMER_10_BIT
#define BACKLIGHT_DUTY_MAX      ((1 << 10) - 1)
#define BACKLIGHT_PWM_HZ        20000
// Dim to this share of the set brightness for the last stretch before sleep
#define BACKLIGHT_DIM_PERCENT   20
#define BACKLIGHT_DIM_LEAD_MS   15000

static volatile uint32_t s_timeout_sec = 300;
static volatile uint8_t s_brightness_pct = 100;
static bool s_dimmed = false;

static esp_lcd_panel_handle_t s_panel_handle = NULL;
static esp_lcd_touch_handle_t s_touch_handle = NULL;
static SemaphoreHandle_t s_lvgl_mutex = NULL;
static SemaphoreHandle_t s_frame_swap_sem = NULL;

static bool s_display_is_asleep = false;
static bool s_ignore_current_touch = false;

static uint32_t lvgl_tick_get_cb(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

// Called from the bounce-buffer ISR each time the driver has finished reading a
// whole frame buffer. At this point it latches the frame buffer selected by the
// last draw_bitmap() call, so the previous one is no longer being scanned out.
static IRAM_ATTR bool on_frame_buf_complete(esp_lcd_panel_handle_t panel,
                                            const esp_lcd_rgb_panel_event_data_t *edata,
                                            void *user_ctx) {
  BaseType_t need_yield = pdFALSE;
  xSemaphoreGiveFromISR(s_frame_swap_sem, &need_yield);
  return need_yield == pdTRUE;
}

// LVGL renders straight into one of the two PSRAM frame buffers (DIRECT mode).
// Once all dirty areas of a frame are drawn, hand that buffer to the driver and
// wait until scan-out has switched to it before letting LVGL draw into the other
// one. This guarantees the CPU never writes the buffer being displayed (tearing).
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area,
                          uint8_t *px_map) {
  (void)area;
  if (lv_display_flush_is_last(disp)) {
    // px_map is a frame buffer, so the driver just switches to it (no copy)
    esp_lcd_panel_draw_bitmap(s_panel_handle, 0, 0, BSP_LCD_H_RES,
                              BSP_LCD_V_RES, px_map);
    // Clear any stale completion *after* queuing the swap: worst case we wait
    // one extra frame, but we can never return before the swap took effect.
    xSemaphoreTake(s_frame_swap_sem, 0);
    xSemaphoreTake(s_frame_swap_sem, pdMS_TO_TICKS(FRAME_SWAP_TIMEOUT_MS));
  }
  lv_display_flush_ready(disp);
}

static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  esp_lcd_touch_point_data_t touch_data[1];
  uint8_t touch_cnt = 0;

  esp_lcd_touch_read_data(s_touch_handle);
  esp_err_t err =
      esp_lcd_touch_get_data(s_touch_handle, touch_data, &touch_cnt, 1);

  if (err == ESP_OK && touch_cnt > 0) {
    // A touch that wakes or un-dims the screen only does that: it is swallowed
    // until the finger lifts, so it can't also press whatever is under it.
    if (s_display_is_asleep) {
      s_display_is_asleep = false;
      s_ignore_current_touch = true;

      // The panel kept scanning out while asleep (only the backlight was off,
      // showing a black frame), so just redraw. Any DMA drift is fixed by
      // CONFIG_LCD_RGB_RESTART_IN_VSYNC.
      ui_view_wake();
      lv_obj_invalidate(lv_screen_active());
      bsp_display_backlight_set(true);
      lv_display_trigger_activity(NULL);
      ESP_LOGI(TAG, "Wake-up touch detected, backlight ON.");
    } else if (s_dimmed && !s_ignore_current_touch) {
      s_ignore_current_touch = true;
      bsp_display_backlight_set(true);   // clears s_dimmed
      lv_display_trigger_activity(NULL); // the swallowed touch doesn't count as input
      ESP_LOGI(TAG, "Touch while dimmed, brightness restored.");
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

static volatile bool s_ota_in_progress = false;

static void backlight_apply_pct(uint8_t pct) {
  uint32_t duty = ((uint32_t)pct * BACKLIGHT_DUTY_MAX) / 100;
  ledc_set_duty(LEDC_LOW_SPEED_MODE, BACKLIGHT_LEDC_CHANNEL, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, BACKLIGHT_LEDC_CHANNEL);
}

static uint8_t dimmed_pct(void) {
  uint8_t pct = (uint8_t)((s_brightness_pct * BACKLIGHT_DIM_PERCENT) / 100);
  return pct > 0 ? pct : 1;
}

// Inactivity handling: dim shortly before the timeout, then sleep. Any touch
// resets LVGL's inactivity timer, which undoes the dim on the next pass.
static void handle_inactivity(void) {
  uint32_t timeout_sec = s_timeout_sec;
  if (timeout_sec == 0 || s_display_is_asleep) {
    if (s_dimmed && timeout_sec == 0) {
      s_dimmed = false;
      backlight_apply_pct(s_brightness_pct);
    }
    return;
  }

  uint32_t inactive_ms = lv_display_get_inactive_time(NULL);
  uint32_t timeout_ms = timeout_sec * 1000;

  if (inactive_ms >= timeout_ms) {
    s_display_is_asleep = true;
    s_dimmed = false;
    bsp_display_backlight_set(false);
    ui_view_sleep();
    ESP_LOGI(TAG, "Screen inactive for %lu s. Backlight OFF.", (unsigned long)timeout_sec);
    return;
  }

  bool should_dim = timeout_ms > BACKLIGHT_DIM_LEAD_MS &&
                    inactive_ms >= timeout_ms - BACKLIGHT_DIM_LEAD_MS;
  if (should_dim != s_dimmed) {
    s_dimmed = should_dim;
    backlight_apply_pct(should_dim ? dimmed_pct() : s_brightness_pct);
  }
}

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

    handle_inactivity();

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

void bsp_display_resume_after_ota(void) {
  s_ota_in_progress = false;
  bsp_display_backlight_set(true);
}

void bsp_display_backlight_set(bool enable) {
  if (enable) s_dimmed = false;
  backlight_apply_pct(enable ? s_brightness_pct : 0);
}

void bsp_display_set_brightness(uint8_t pct) {
  if (pct < 1) pct = 1;
  if (pct > 100) pct = 100;
  s_brightness_pct = pct;
  if (!s_display_is_asleep && !s_ota_in_progress) {
    backlight_apply_pct(s_dimmed ? dimmed_pct() : pct);
  }
}

uint8_t bsp_display_get_brightness(void) {
  return s_brightness_pct;
}

void bsp_display_set_timeout(uint32_t seconds) {
  s_timeout_sec = seconds;
}

uint32_t bsp_display_get_timeout(void) {
  return s_timeout_sec;
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
  s_frame_swap_sem = xSemaphoreCreateBinary();

  // 1. Backlight PWM (LEDC) so brightness can be set and dimmed before sleep
  ledc_timer_config_t bk_timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = BACKLIGHT_LEDC_RES,
      .timer_num = BACKLIGHT_LEDC_TIMER,
      .freq_hz = BACKLIGHT_PWM_HZ,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_ERROR_CHECK(ledc_timer_config(&bk_timer));
  ledc_channel_config_t bk_channel = {
      .gpio_num = BSP_LCD_BK_LIGHT,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = BACKLIGHT_LEDC_CHANNEL,
      .timer_sel = BACKLIGHT_LEDC_TIMER,
      .duty = 0,
      .hpoint = 0,
  };
  ESP_ERROR_CHECK(ledc_channel_config(&bk_channel));
  bsp_display_backlight_set(true);

  // 2. ST7262 RGB Panel Driver Init
  esp_lcd_rgb_panel_config_t rgb_config = {
      .clk_src = LCD_CLK_SRC_PLL160M,
      .dma_burst_size = 64,
      .timings =
          {
              .pclk_hz = LCD_PCLK_HZ,
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
      // Two PSRAM frame buffers: LVGL draws into one while the other is shown
      .num_fbs = 2,
      // Bounce buffers in internal RAM decouple scan-out from PSRAM latency
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
          },
  };

  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&rgb_config, &s_panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));

  esp_lcd_rgb_panel_event_callbacks_t panel_cbs = {
      .on_frame_buf_complete = on_frame_buf_complete,
  };
  ESP_ERROR_CHECK(
      esp_lcd_rgb_panel_register_event_callbacks(s_panel_handle, &panel_cbs, NULL));

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

  // 4. LVGL renders directly into the driver's two frame buffers. In DIRECT
  // mode LVGL only redraws dirty areas and copies them to the other buffer
  // after each swap, so both stay in sync.
  lv_init();

  void *fb0 = NULL;
  void *fb1 = NULL;
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(s_panel_handle, 2, &fb0, &fb1));
  size_t fb_size = BSP_LCD_H_RES * BSP_LCD_V_RES * sizeof(uint16_t);

  lv_display_t *disp = lv_display_create(BSP_LCD_H_RES, BSP_LCD_V_RES);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(disp, lvgl_flush_cb);
  // fb0 is on screen at start-up, so let LVGL draw the first frame into fb1
  lv_display_set_buffers(disp, fb1, fb0, fb_size,
                         LV_DISPLAY_RENDER_MODE_DIRECT);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, lvgl_touch_read_cb);
  lv_timer_t *read_timer = lv_indev_get_read_timer(indev);
  if (read_timer) {
    lv_timer_set_period(read_timer, 10);
  }

  // 5. LVGL tick source: read the system timer on demand instead of a 2 ms
  // periodic interrupt
  lv_tick_set_cb(lvgl_tick_get_cb);

  // Create FreeRTOS task to run lv_timer_handler()
  xTaskCreatePinnedToCore(lvgl_task, "LVGL Task", 16384, NULL, 5, NULL, 1);

  ESP_LOGI(TAG, "Display BSP successfully initialized!");
  return ESP_OK;
}
