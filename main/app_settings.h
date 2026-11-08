#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load display settings from NVS and apply them. Call after bsp_display_init().
 */
void app_settings_load(void);

/**
 * @brief Set and save the screen inactivity timeout in seconds (0 = never sleep).
 */
esp_err_t app_settings_set_timeout(uint32_t seconds);

/**
 * @brief Set and save the backlight brightness, 1-100 %.
 */
esp_err_t app_settings_set_brightness(uint8_t pct);

/**
 * @brief Register the "display" and "game" serial console commands.
 */
void app_settings_register_console_cmds(void);

#ifdef __cplusplus
}
#endif

#endif // APP_SETTINGS_H
