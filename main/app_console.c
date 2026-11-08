#include "app_console.h"
#include "app_wifi.h"
#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "esp_log.h"
#include "wled_espnow.h"
#include "app_settings.h"

static const char *TAG = "app_console";

static int cmd_wifi(int argc, char **argv) {
    if (argc == 1) {
        const char *ssid = app_wifi_get_ssid();
        printf("Wi-Fi: %s\n", ssid[0] ? ssid : "(not configured)");
        printf("  Status: %s\n", app_wifi_get_ip_str());
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "forget") == 0) {
        esp_err_t err = app_wifi_forget();
        if (err == ESP_OK) {
            printf("Credentials erased\n");
        } else {
            printf("Failed: %s\n", esp_err_to_name(err));
        }
        return err == ESP_OK ? 0 : 1;
    }
    if (argc == 2 || argc == 3) {
        esp_err_t err = app_wifi_connect(argv[1], argc == 3 ? argv[2] : "");
        if (err == ESP_OK) {
            printf("Connecting to '%s'...\n", argv[1]);
        } else {
            printf("Failed: %s\n", esp_err_to_name(err));
        }
        return err == ESP_OK ? 0 : 1;
    }
    printf("Usage: wifi | wifi <ssid> [password] | wifi forget\n"
           "       (quote values containing spaces)\n");
    return 1;
}

static int cmd_ota(int argc, char **argv) {
    if (argc == 1) {
        printf("OTA uploads: %s\n", app_wifi_has_ota_token() ? "enabled (token set)"
                                                               : "disabled (no token)");
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "token") == 0) {
        bool clear = strcmp(argv[2], "clear") == 0;
        esp_err_t err = app_wifi_set_ota_token(clear ? NULL : argv[2]);
        if (err != ESP_OK) {
            printf("Failed: %s\n", esp_err_to_name(err));
            return 1;
        }
        printf(clear ? "Token cleared; OTA uploads disabled\n" : "Token saved\n");
        return 0;
    }
    printf("Usage:\n"
           "  ota                  Show whether OTA uploads are enabled\n"
           "  ota token <secret>   Require this token for uploads (max 64 chars)\n"
           "  ota token clear      Remove the token (disables OTA uploads)\n");
    return 1;
}

void app_console_start(void) {
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "darts>";
    repl_config.max_cmdline_length = 128;

    esp_console_register_help_command();
    wled_espnow_register_console_cmd();
    const esp_console_cmd_t wifi_cmd = {
        .command = "wifi",
        .help = "Show Wi-Fi status, set credentials (wifi <ssid> [password]) or 'wifi forget'",
        .func = cmd_wifi,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_cmd));
    const esp_console_cmd_t ota_cmd = {
        .command = "ota",
        .help = "OTA upload token ('ota token <secret>' / 'ota token clear')",
        .func = cmd_ota,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&ota_cmd));
    app_settings_register_console_cmds();

    esp_err_t err;
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    err = esp_console_new_repl_uart(&hw_config, &repl_config, &repl);
#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    err = esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl);
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    err = esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl);
#else
    ESP_LOGW(TAG, "No console device configured; serial commands unavailable");
    return;
#endif
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create console REPL: %s", esp_err_to_name(err));
        return;
    }
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
