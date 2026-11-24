#ifndef BSP_DISPLAY_H
#define BSP_DISPLAY_H

#include "esp_err.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_LCD_H_RES 800
#define BSP_LCD_V_RES 480
#define BSP_LCD_BK_LIGHT 2

// ST7262 RGB Panel Pins
#define BSP_LCD_HSYNC 39
#define BSP_LCD_VSYNC 41
#define BSP_LCD_DE 40
#define BSP_LCD_PCLK 42

#define BSP_LCD_DATA_R0 8
#define BSP_LCD_DATA_R1 3
#define BSP_LCD_DATA_R2 46
#define BSP_LCD_DATA_R3 9
#define BSP_LCD_DATA_R4 1

#define BSP_LCD_DATA_G0 5
#define BSP_LCD_DATA_G1 6
#define BSP_LCD_DATA_G2 7
#define BSP_LCD_DATA_G3 15
#define BSP_LCD_DATA_G4 16
#define BSP_LCD_DATA_G5 4

#define BSP_LCD_DATA_B0 45
#define BSP_LCD_DATA_B1 48
#define BSP_LCD_DATA_B2 47
#define BSP_LCD_DATA_B3 21
#define BSP_LCD_DATA_B4 14

// GT911 I2C Touch Controller Pins
#define BSP_I2C_SDA 19
#define BSP_I2C_SCL 20
#define BSP_TOUCH_RST 38
#define BSP_TOUCH_INT 18
#define BSP_TOUCH_I2C_ADDR 0x5D

/**
 * @brief Initialize display hardware, touch panel, and LVGL stack.
 * @return ESP_OK on success.
 */
esp_err_t bsp_display_init(void);

/**
 * @brief Turn backlight on (at the configured brightness) or off.
 */
void bsp_display_backlight_set(bool enable);

/**
 * @brief Set backlight brightness, 1-100 %. Not persisted (see app_settings).
 */
void bsp_display_set_brightness(uint8_t pct);
uint8_t bsp_display_get_brightness(void);

/**
 * @brief Set the inactivity timeout before the screen sleeps (0 = never).
 *        The backlight dims for the last 15 s before sleeping.
 */
void bsp_display_set_timeout(uint32_t seconds);
uint32_t bsp_display_get_timeout(void);

/**
 * @brief Put the display to sleep now (same as the inactivity timeout: black
 *        frame, backlight off, WLED off). Applied on the LVGL task's next
 *        pass, so it's safe to call from LVGL event handlers. A touch wakes it.
 */
void bsp_display_request_sleep(void);

/**
 * @brief Pause LVGL processing and feed watchdog during OTA update.
 */
void bsp_display_pause_for_ota(void);

/**
 * @brief Resume LVGL processing and backlight after a failed OTA update.
 */
void bsp_display_resume_after_ota(void);

/**
 * @brief Lock LVGL mutex before accessing LVGL APIs (if multithreaded).
 */
void bsp_display_lock(void);

/**
 * @brief Unlock LVGL mutex.
 */
void bsp_display_unlock(void);

#ifdef __cplusplus
}
#endif

#endif // BSP_DISPLAY_H
