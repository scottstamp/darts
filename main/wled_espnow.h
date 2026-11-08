#ifndef WLED_ESPNOW_H
#define WLED_ESPNOW_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load config from NVS and start ESP-NOW if the link is enabled.
 *        Call after nvs_flash_init(). Safe to call before or after app_wifi_init().
 */
void wled_espnow_init(void);

/**
 * @brief Turn the WLED controller on/off (follows the display). Sent only when
 *        the state changes. Non-blocking; safe to call from the LVGL task.
 */
void wled_espnow_set_power(bool on);

/**
 * @brief Register the "wled" serial console command.
 */
void wled_espnow_register_console_cmd(void);

#ifdef __cplusplus
}
#endif

#endif // WLED_ESPNOW_H
