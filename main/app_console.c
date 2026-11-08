#include "app_console.h"
#include "esp_console.h"
#include "esp_log.h"
#include "wled_espnow.h"

static const char *TAG = "app_console";

void app_console_start(void) {
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "darts>";
    repl_config.max_cmdline_length = 128;

    esp_console_register_help_command();
    wled_espnow_register_console_cmd();

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
