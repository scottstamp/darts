#include "app_wifi.h"
#include "bsp_display.h"
#include "ui_view.h"
#include <string.h>
#include <sys/param.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#if __has_include("mdns.h")
#include "mdns.h"
#define HAVE_MDNS 1
#else
#define HAVE_MDNS 0
#endif

static const char *TAG = "app_wifi";

static app_wifi_state_t s_wifi_state = APP_WIFI_STATE_DISCONNECTED;
static char s_ip_str[32] = "Disconnected";
static char s_ssid[33] = {0};
static char s_password[65] = {0};
static httpd_handle_t s_http_server = NULL;
static bool s_is_sta_initialized = false;

static const char *INDEX_HTML =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Darts Scoreboard OTA</title>"
    "<style>"
    "body{font-family:-apple-system,BlinkMacSystemFont,Roboto,Helvetica,Arial,sans-serif;background:#121218;color:#fff;margin:0;padding:24px;display:flex;justify-content:center;align-items:center;min-height:100vh;}"
    ".card{background:#1e1e28;border-radius:16px;padding:32px;max-width:440px;width:100%;box-shadow:0 8px 32px rgba(0,0,0,0.4);border:1px solid #282836;text-align:center;}"
    "h1{color:#3b82f6;margin-top:0;font-size:24px;}"
    ".status{background:#181820;padding:12px 16px;border-radius:8px;font-size:14px;margin:16px 0;text-align:left;border:1px solid #2a2a3a;}"
    ".status-val{float:right;font-weight:600;color:#10b981;}"
    "input[type=file]{display:none;}"
    ".upload-btn{display:block;background:#3b82f6;color:#fff;padding:14px 20px;border-radius:8px;font-weight:600;cursor:pointer;margin-top:20px;transition:0.2s;}"
    ".upload-btn:hover{background:#2563eb;}"
    "#progress{margin-top:16px;height:8px;background:#2a2a3a;border-radius:4px;overflow:hidden;display:none;}"
    "#bar{height:100%;width:0%;background:#10b981;transition:width 0.1s;}"
    "</style></head><body>"
    "<div class='card'>"
    "<h1>Darts Scoreboard OTA</h1>"
    "<div class='status'>Status: <span class='status-val'>Connected</span></div>"
    "<div class='status'>IP Address: <span class='status-val' id='ip'>%s</span></div>"
    "<div class='status'>Active Partition: <span class='status-val'>%s</span></div>"
    "<form id='upload_form' action='/update' method='post' enctype='multipart/form-data'>"
    "<label for='file_input' class='upload-btn'>Select Firmware (.bin)</label>"
    "<input type='file' id='file_input' name='update'>"
    "</form>"
    "<div id='progress'><div id='bar'></div></div>"
    "<div id='msg' style='margin-top:16px;font-size:14px;color:#a0a0af;'></div>"
    "</div>"
    "<script>"
    "document.getElementById('file_input').onchange = function() {"
    "  var file = this.files[0];"
    "  if(!file) return;"
    "  var xhr = new XMLHttpRequest();"
    "  var progress = document.getElementById('progress');"
    "  var bar = document.getElementById('bar');"
    "  var msg = document.getElementById('msg');"
    "  progress.style.display = 'block';"
    "  msg.innerText = 'Uploading firmware... Please wait.';"
    "  xhr.upload.onprogress = function(e) {"
    "    if(e.lengthComputable) { var p = Math.round((e.loaded / e.total) * 100); bar.style.width = p + '%%'; }"
    "  };"
    "  xhr.onload = function() {"
    "    if(xhr.status == 200) { msg.innerText = 'Update Successful! Rebooting...'; setTimeout(function(){ location.reload(); }, 5000); }"
    "    else { msg.innerText = 'Upload Failed: ' + xhr.responseText; }"
    "  };"
    "  xhr.open('POST', '/update');"
    "  xhr.send(file);"
    "};"
    "</script></body></html>";

static esp_err_t index_get_handler(httpd_req_t *req) {
    const esp_partition_t *running = esp_ota_get_running_partition();
    char resp[2048];
    snprintf(resp, sizeof(resp), INDEX_HTML, s_ip_str, running ? running->label : "Unknown");
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t ota_post_handler(httpd_req_t *req) {
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No OTA partition available");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition available");
        return ESP_FAIL;
    }

    // Turn off screen backlight and fill framebuffer with solid black during OTA update
    ui_view_prepare_for_ota();

    ESP_LOGI(TAG, "Starting OTA update onto partition: %s (size: %d bytes)", update_partition->label, req->content_len);
    esp_err_t err = esp_ota_begin(update_partition, req->content_len, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        bsp_display_backlight_set(true);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA Begin Failed");
        return ESP_FAIL;
    }

    char *buf = malloc(16384);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate 16KB OTA buffer");
        esp_ota_abort(ota_handle);
        bsp_display_backlight_set(true);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory Allocation Failed");
        return ESP_FAIL;
    }

    int received;
    int remaining = req->content_len;

    while (remaining > 0) {
        if ((received = httpd_req_recv(req, buf, MIN(remaining, 16384))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
            ESP_LOGE(TAG, "HTTP receive failed");
            free(buf);
            esp_ota_abort(ota_handle);
            bsp_display_backlight_set(true);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive Failed");
            return ESP_FAIL;
        }

        err = esp_ota_write(ota_handle, (const void *)buf, received);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
            free(buf);
            esp_ota_abort(ota_handle);
            bsp_display_backlight_set(true);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA Write Failed");
            return ESP_FAIL;
        }

        remaining -= received;
    }
    free(buf);

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
        bsp_display_backlight_set(true);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA End Failed");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
        bsp_display_backlight_set(true);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set Boot Partition Failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA Success! Boot partition set to %s. Rebooting in 1s...", update_partition->label);
    httpd_resp_sendstr(req, "OK");

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

static void start_web_server(void) {
    if (s_http_server) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = 8;

    if (httpd_start(&s_http_server, &config) == ESP_OK) {
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(s_http_server, &index_uri);

        httpd_uri_t update_uri = {
            .uri = "/update",
            .method = HTTP_POST,
            .handler = ota_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(s_http_server, &update_uri);

        httpd_uri_t ota_uri = {
            .uri = "/ota",
            .method = HTTP_POST,
            .handler = ota_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(s_http_server, &ota_uri);

        ESP_LOGI(TAG, "OTA Web Server started on port %d", config.server_port);
    }
}

static void start_mdns(void) {
#if HAVE_MDNS
    esp_err_t err = mdns_init();
    if (err == ESP_OK) {
        mdns_hostname_set("darts-scoreboard");
        mdns_instance_name_set("Darts Scoreboard OTA");
        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
        ESP_LOGI(TAG, "mDNS active: http://darts-scoreboard.local/");
    }
#endif
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        s_wifi_state = APP_WIFI_STATE_CONNECTING;
        snprintf(s_ip_str, sizeof(s_ip_str), "Connecting...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_state = APP_WIFI_STATE_FAILED;
        snprintf(s_ip_str, sizeof(s_ip_str), "Disconnected");
        ESP_LOGW(TAG, "Wi-Fi disconnected. Reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_state = APP_WIFI_STATE_CONNECTED;
        ESP_LOGI(TAG, "Wi-Fi Connected! IP: %s", s_ip_str);
        start_mdns();
        start_web_server();
    }
}

void app_wifi_init(void) {
    // Restore saved credentials from NVS
    nvs_handle_t handle;
    if (nvs_open("app_wifi", NVS_READWRITE, &handle) == ESP_OK) {
        size_t s_len = sizeof(s_ssid);
        size_t p_len = sizeof(s_password);
        if (nvs_get_str(handle, "ssid", s_ssid, &s_len) != ESP_OK || strlen(s_ssid) == 0) {
            s_ssid[0] = '\0';
            s_password[0] = '\0';
        } else {
            nvs_get_str(handle, "pass", s_password, &p_len);
        }
        nvs_close(handle);
    } else {
        s_ssid[0] = '\0';
        s_password[0] = '\0';
    }

    if (strlen(s_ssid) > 0) {
        app_wifi_connect(s_ssid, s_password);
    }
}

esp_err_t app_wifi_connect(const char *ssid, const char *password) {
    if (!ssid || strlen(ssid) == 0) return ESP_ERR_INVALID_ARG;

    // Save credentials to NVS
    nvs_handle_t handle;
    if (nvs_open("app_wifi", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, "ssid", ssid);
        nvs_set_str(handle, "pass", password ? password : "");
        nvs_commit(handle);
        nvs_close(handle);
    }

    snprintf(s_ssid, sizeof(s_ssid), "%s", ssid);
    snprintf(s_password, sizeof(s_password), "%s", password ? password : "");

    if (!s_is_sta_initialized) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        s_is_sta_initialized = true;
    }

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, s_ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, s_password, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    s_wifi_state = APP_WIFI_STATE_CONNECTING;
    snprintf(s_ip_str, sizeof(s_ip_str), "Connecting...");
    return ESP_OK;
}

app_wifi_state_t app_wifi_get_state(void) {
    return s_wifi_state;
}

const char *app_wifi_get_ip_str(void) {
    return s_ip_str;
}

const char *app_wifi_get_ssid(void) {
    return s_ssid;
}
