#ifndef APP_WIFI_H
#define APP_WIFI_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_WIFI_STATE_DISCONNECTED,
    APP_WIFI_STATE_CONNECTING,
    APP_WIFI_STATE_CONNECTED,
    APP_WIFI_STATE_FAILED
} app_wifi_state_t;

/**
 * @brief Initialize Wi-Fi subsystem, restore saved credentials from NVS, and start connection/OTA server.
 */
void app_wifi_init(void);

/**
 * @brief Save Wi-Fi credentials to NVS and initiate connection.
 */
esp_err_t app_wifi_connect(const char *ssid, const char *password);

/**
 * @brief Erase saved credentials from NVS and disconnect. Wi-Fi stays off after reboot.
 */
esp_err_t app_wifi_forget(void);

/**
 * @brief Set the token required (X-OTA-Token header) for OTA uploads; NULL or
 *        "" clears it, which disables OTA. Max 64 characters. Saved to NVS.
 */
esp_err_t app_wifi_set_ota_token(const char *token);

/**
 * @brief Whether an OTA token is configured (OTA uploads are refused otherwise).
 */
bool app_wifi_has_ota_token(void);

/**
 * @brief Get current Wi-Fi connection state.
 */
app_wifi_state_t app_wifi_get_state(void);

/**
 * @brief Get current IP address string (e.g. "192.168.1.120" or "Disconnected").
 */
const char *app_wifi_get_ip_str(void);

/**
 * @brief Get current connected SSID string.
 */
const char *app_wifi_get_ssid(void);

#ifdef __cplusplus
}
#endif

#endif // APP_WIFI_H
