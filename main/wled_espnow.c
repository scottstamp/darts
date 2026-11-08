#include "wled_espnow.h"
#include "app_wifi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "esp_check.h"
#include "esp_console.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

/*
 * WLED ESP-NOW link: switches the WLED controller on/off with the display.
 *
 * Uses WLED's built-in WiZmote remote protocol: a 13-byte ESP-NOW packet whose
 * first byte is 0x91 (ON button) or 0x81 (any other button). WLED only accepts
 * it when the sender MAC matches the "linked remote" MAC configured in WLED
 * (Config -> WiFi Setup -> ESP-NOW), and drops packets whose sequence number
 * equals the previous one, so repeats of the same packet are harmless.
 *
 * All radio/ESP-NOW calls run on a single sender task; other tasks only post
 * messages to its queue.
 */

static const char *TAG = "wled_espnow";

#define NVS_NAMESPACE   "wled"
#define NVS_KEY_CFG     "cfg"
#define CFG_VERSION     3
#define SEND_ATTEMPTS   3
#define ACK_TIMEOUT_MS  100
#define BUTTON_ON       1   // WiZmote ON button
#define BUTTON_OFF      2   // WiZmote OFF button
#define NVS_KEY_RESYNC  "resync"
// Re-send the current on/off state this often, so WLED recovers if it was
// power-cycled or switched from elsewhere (0 = only send on changes)
#define DEFAULT_RESYNC_SEC 120
#define MAX_RESYNC_SEC     3600

typedef struct __attribute__((packed)) {
    uint8_t program;   // 0x91 for ON, 0x81 for everything else
    uint8_t seq[4];    // Sequence number, LSB first
    uint8_t dt1;       // Button data type (0x32)
    uint8_t button;    // Button code
    uint8_t dt2;       // Battery data type (0x01)
    uint8_t bat_level; // Battery level 0-100
    uint8_t reserved[4];
} wizmote_msg_t;

_Static_assert(sizeof(wizmote_msg_t) == 13, "WLED requires a 13-byte WiZmote packet");

typedef struct {
    uint8_t version;
    uint8_t enabled;
    uint8_t channel;
    uint8_t peer[ESP_NOW_ETH_ALEN];
} wled_cfg_t;

typedef enum {
    MSG_POWER,     // val = desired power (follows display), sent only on change
    MSG_RAW_CODE,  // val = button code, sent unconditionally (console test)
    MSG_RECONFIG,  // re-apply config (enable/disable, peer, channel)
} msg_kind_t;

typedef struct {
    uint8_t kind;
    uint8_t val;
} wled_msg_t;

static const uint8_t BROADCAST_MAC[ESP_NOW_ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static wled_cfg_t s_cfg;
static SemaphoreHandle_t s_cfg_lock;
static QueueHandle_t s_queue;
static TaskHandle_t s_task;

// Owned by the sender task
static bool s_radio_up;
static bool s_own_wifi;           // We initialised the Wi-Fi driver (not app_wifi)
static uint8_t s_cur_peer[ESP_NOW_ETH_ALEN];
static bool s_have_peer;
static int s_want_on = -1;        // Latest requested power state (-1 = unknown)
static int s_sent_on = -1;        // Latest power state delivered to WLED
static uint32_t s_seq;
static volatile uint32_t s_tx_ok, s_tx_fail;
// Stored under its own NVS key so existing link configs stay valid
static volatile uint16_t s_resync_sec = DEFAULT_RESYNC_SEC;

static void cfg_set_defaults(wled_cfg_t *c) {
    memset(c, 0, sizeof(*c));
    c->version = CFG_VERSION;
    c->enabled = 0;
    c->channel = 1;
    memcpy(c->peer, BROADCAST_MAC, sizeof(c->peer));
}

static void cfg_load(void) {
    cfg_set_defaults(&s_cfg);
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return;
    wled_cfg_t c;
    size_t len = sizeof(c);
    if (nvs_get_blob(h, NVS_KEY_CFG, &c, &len) == ESP_OK && len == sizeof(c) && c.version == CFG_VERSION) {
        s_cfg = c;
    }
    uint16_t resync;
    if (nvs_get_u16(h, NVS_KEY_RESYNC, &resync) == ESP_OK && resync <= MAX_RESYNC_SEC) {
        s_resync_sec = resync;
    }
    nvs_close(h);
}

static esp_err_t resync_save(uint16_t seconds) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u16(h, NVS_KEY_RESYNC, seconds);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static esp_err_t cfg_save(const wled_cfg_t *c) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, NVS_KEY_CFG, c, sizeof(*c));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static void cfg_get(wled_cfg_t *out) {
    xSemaphoreTake(s_cfg_lock, portMAX_DELAY);
    *out = s_cfg;
    xSemaphoreGive(s_cfg_lock);
}

static bool post(msg_kind_t kind, uint8_t val, TickType_t wait) {
    if (!s_queue) return false;
    wled_msg_t m = {.kind = kind, .val = val};
    return xQueueSend(s_queue, &m, wait) == pdTRUE;
}

/* ---------------- Radio management (sender task only) ---------------- */

static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    (void)tx_info;
    if (s_task) {
        // +1 so that a zero notification value means "no callback yet"
        xTaskNotify(s_task, (uint32_t)status + 1, eSetValueWithOverwrite);
    }
}

static esp_err_t radio_start(void) {
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_ERR_WIFI_NOT_INIT) {
        esp_err_t err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
        // Create the STA netif so app_wifi can reuse it later if Wi-Fi is turned on.
        if (!esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")) {
            esp_netif_create_default_wifi_sta();
        }
        wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_RETURN_ON_ERROR(esp_wifi_init(&wcfg), TAG, "esp_wifi_init");
        ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "set_storage");
        ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set_mode");
        s_own_wifi = true;
    }
    if (s_own_wifi) {
        ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start");
    }
    ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "esp_now_init");
    ESP_RETURN_ON_ERROR(esp_now_register_send_cb(espnow_send_cb), TAG, "register_send_cb");
    s_have_peer = false;
    s_radio_up = true;
    ESP_LOGI(TAG, "ESP-NOW started");
    return ESP_OK;
}

static void radio_stop(void) {
    if (!s_radio_up) return;
    esp_now_deinit();
    s_have_peer = false;
    s_radio_up = false;
    // Only power the radio down if nobody else is using Wi-Fi.
    if (s_own_wifi && app_wifi_get_state() == APP_WIFI_STATE_DISCONNECTED) {
        esp_wifi_stop();
    }
    ESP_LOGI(TAG, "ESP-NOW stopped");
}

static void radio_apply(const wled_cfg_t *cfg) {
    if (!cfg->enabled) {
        radio_stop();
        return;
    }
    if (!s_radio_up) {
        esp_err_t err = radio_start();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start ESP-NOW: %s", esp_err_to_name(err));
            return;
        }
    }

    // Channel: only settable while the STA isn't associated with an AP.
    // When connected, ESP-NOW follows the AP channel (WLED must be on the same one).
    app_wifi_state_t ws = app_wifi_get_state();
    if (ws == APP_WIFI_STATE_CONNECTED || ws == APP_WIFI_STATE_CONNECTING) {
        ESP_LOGW(TAG, "Wi-Fi STA active; using AP channel instead of configured channel %u", cfg->channel);
    } else {
        esp_err_t err = esp_wifi_set_channel(cfg->channel, WIFI_SECOND_CHAN_NONE);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_set_channel(%u) failed: %s", cfg->channel, esp_err_to_name(err));
        }
    }

    if (s_have_peer && memcmp(s_cur_peer, cfg->peer, ESP_NOW_ETH_ALEN) != 0) {
        esp_now_del_peer(s_cur_peer);
        s_have_peer = false;
    }
    if (!s_have_peer) {
        esp_now_peer_info_t peer = {0};
        memcpy(peer.peer_addr, cfg->peer, ESP_NOW_ETH_ALEN);
        peer.channel = 0; // current channel
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;
        esp_err_t err = esp_now_add_peer(&peer);
        if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
            ESP_LOGE(TAG, "esp_now_add_peer failed: %s", esp_err_to_name(err));
            return;
        }
        memcpy(s_cur_peer, cfg->peer, ESP_NOW_ETH_ALEN);
        s_have_peer = true;
    }
}

static bool send_code(const wled_cfg_t *cfg, uint8_t code) {
    if (!s_radio_up || !s_have_peer) return false;

    wizmote_msg_t m = {0};
    s_seq++;
    m.program = (code == BUTTON_ON) ? 0x91 : 0x81;
    m.seq[0] = s_seq & 0xff;
    m.seq[1] = (s_seq >> 8) & 0xff;
    m.seq[2] = (s_seq >> 16) & 0xff;
    m.seq[3] = (s_seq >> 24) & 0xff;
    m.dt1 = 0x32;
    m.button = code;
    m.dt2 = 0x01;
    m.bat_level = 100;

    // Unicast: retry until ACKed. Broadcast: no ACKs, so always send every
    // repeat (WLED de-duplicates by sequence number).
    bool bcast = memcmp(cfg->peer, BROADCAST_MAC, ESP_NOW_ETH_ALEN) == 0;
    bool delivered = false;
    for (int i = 0; i < SEND_ATTEMPTS; i++) {
        xTaskNotifyStateClear(NULL);
        ulTaskNotifyValueClear(NULL, UINT32_MAX);
        esp_err_t err = esp_now_send(cfg->peer, (const uint8_t *)&m, sizeof(m));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        uint32_t st = 0;
        if (xTaskNotifyWait(0, UINT32_MAX, &st, pdMS_TO_TICKS(ACK_TIMEOUT_MS)) == pdTRUE &&
            st == (uint32_t)ESP_NOW_SEND_SUCCESS + 1) {
            delivered = true;
            if (!bcast) break;
        }
        if (bcast) vTaskDelay(pdMS_TO_TICKS(20));
    }

    const char *name = (code == BUTTON_ON) ? "ON" : "OFF";
    if (delivered) {
        s_tx_ok++;
        ESP_LOGI(TAG, "Sent %s (seq %lu)", name, (unsigned long)s_seq);
    } else {
        s_tx_fail++;
        ESP_LOGW(TAG, "%s not acknowledged by WLED", name);
    }
    return delivered;
}

static void sync_power(const wled_cfg_t *cfg) {
    if (!cfg->enabled || s_want_on < 0 || s_want_on == s_sent_on) return;
    if (send_code(cfg, s_want_on ? BUTTON_ON : BUTTON_OFF)) {
        s_sent_on = s_want_on;
    }
}

static void wled_task(void *arg) {
    (void)arg;
    wled_msg_t msg;
    for (;;) {
        uint16_t resync = s_resync_sec;
        TickType_t wait = resync ? pdMS_TO_TICKS((uint32_t)resync * 1000) : portMAX_DELAY;
        if (xQueueReceive(s_queue, &msg, wait) != pdTRUE) {
            // Idle for a resync period: re-send the current state (WLED ignores
            // repeats, so this only matters if it lost or changed state)
            wled_cfg_t cfg;
            cfg_get(&cfg);
            if (cfg.enabled && s_want_on >= 0) {
                s_sent_on = -1;
                sync_power(&cfg);
            }
            continue;
        }
        wled_cfg_t cfg;
        cfg_get(&cfg);

        switch (msg.kind) {
        case MSG_POWER:
            s_want_on = msg.val ? 1 : 0;
            sync_power(&cfg);
            break;
        case MSG_RAW_CODE:
            if (!cfg.enabled) {
                ESP_LOGW(TAG, "Link disabled; run 'wled enable' first");
            } else {
                send_code(&cfg, msg.val);
            }
            break;
        case MSG_RECONFIG:
            radio_apply(&cfg);
            s_sent_on = -1; // Re-sync WLED with the current display state
            sync_power(&cfg);
            break;
        }
    }
}

/* ---------------- Public API ---------------- */

void wled_espnow_init(void) {
    if (s_queue) return;
    s_cfg_lock = xSemaphoreCreateMutex();
    cfg_load();
    s_seq = esp_random(); // Avoid matching WLED's last-seen sequence after a reboot
    s_queue = xQueueCreate(16, sizeof(wled_msg_t));
    xTaskCreate(wled_task, "wled_espnow", 4096, NULL, 4, &s_task);
    if (s_cfg.enabled) {
        post(MSG_RECONFIG, 0, portMAX_DELAY);
    }
}

void wled_espnow_set_power(bool on) {
    if (!s_queue) return; // Not initialised yet
    if (!post(MSG_POWER, on ? 1 : 0, 0)) {
        ESP_LOGW(TAG, "Queue full; dropped power %s", on ? "on" : "off");
    }
}

/* ---------------- Serial console ---------------- */

static bool parse_mac(const char *s, uint8_t out[ESP_NOW_ETH_ALEN]) {
    if (strcasecmp(s, "broadcast") == 0) {
        memcpy(out, BROADCAST_MAC, ESP_NOW_ETH_ALEN);
        return true;
    }
    // Accept "aa:bb:cc:dd:ee:ff", "aa-bb-...", or "aabbccddeeff"
    char hex[13];
    int n = 0;
    for (const char *p = s; *p; p++) {
        if (*p == ':' || *p == '-') continue;
        if (n >= 12 || !((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F'))) {
            return false;
        }
        hex[n++] = *p;
    }
    if (n != 12) return false;
    hex[12] = '\0';
    for (int i = 0; i < ESP_NOW_ETH_ALEN; i++) {
        char byte[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
        out[i] = (uint8_t)strtoul(byte, NULL, 16);
    }
    return true;
}

static bool parse_u8(const char *s, long min, long max, uint8_t *out) {
    char *end;
    long v = strtol(s, &end, 0);
    if (*s == '\0' || *end != '\0' || v < min || v > max) return false;
    *out = (uint8_t)v;
    return true;
}

static void print_status(void) {
    wled_cfg_t c;
    cfg_get(&c);
    uint8_t own[6];
    esp_read_mac(own, ESP_MAC_WIFI_STA);
    printf("WLED ESP-NOW link: %s (radio %s)\n", c.enabled ? "enabled" : "disabled", s_radio_up ? "up" : "down");
    printf("  This device MAC : " MACSTR "  (enter %02x%02x%02x%02x%02x%02x in WLED as the linked remote)\n",
           MAC2STR(own), own[0], own[1], own[2], own[3], own[4], own[5]);
    printf("  WLED peer MAC   : " MACSTR "%s\n", MAC2STR(c.peer),
           memcmp(c.peer, BROADCAST_MAC, 6) == 0 ? " (broadcast)" : "");
    printf("  Channel         : %u\n", c.channel);
    printf("  WLED power      : %s\n", s_want_on < 0 ? "-" : (s_want_on ? "on" : "off"));
    if (s_resync_sec) {
        printf("  Resync every    : %u s\n", s_resync_sec);
    } else {
        printf("  Resync every    : off\n");
    }
    printf("  TX ok/fail      : %lu/%lu\n", (unsigned long)s_tx_ok, (unsigned long)s_tx_fail);
}

static void print_usage(void) {
    printf("Usage:\n"
           "  wled                      Show status\n"
           "  wled enable | disable     Turn the link on/off\n"
           "  wled mac <mac|broadcast>  Set WLED controller MAC (e.g. aa:bb:cc:dd:ee:ff)\n"
           "  wled channel <1-14>       Set Wi-Fi channel (must match WLED's)\n"
           "  wled on | off             Send ON/OFF now (test)\n"
           "  wled resync <seconds>     Re-send current state this often (0 = off, max 3600)\n"
           "  wled defaults             Restore default settings\n");
}

static int cmd_wled(int argc, char **argv) {
    if (!s_queue) {
        printf("WLED module not initialised\n");
        return 1;
    }
    if (argc < 2 || strcmp(argv[1], "status") == 0) {
        print_status();
        return 0;
    }

    const char *sub = argv[1];
    wled_cfg_t c;
    cfg_get(&c);

    if (strcmp(sub, "enable") == 0 || strcmp(sub, "disable") == 0) {
        c.enabled = (sub[0] == 'e');
    } else if (strcmp(sub, "mac") == 0 && argc == 3) {
        if (!parse_mac(argv[2], c.peer)) {
            printf("Invalid MAC '%s'\n", argv[2]);
            return 1;
        }
    } else if (strcmp(sub, "channel") == 0 && argc == 3) {
        if (!parse_u8(argv[2], 1, 14, &c.channel)) {
            printf("Channel must be 1-14\n");
            return 1;
        }
    } else if (strcmp(sub, "resync") == 0 && argc == 3) {
        char *end;
        long v = strtol(argv[2], &end, 10);
        if (*argv[2] == '\0' || *end != '\0' || v < 0 || v > MAX_RESYNC_SEC) {
            printf("Resync must be 0-%d seconds\n", MAX_RESYNC_SEC);
            return 1;
        }
        s_resync_sec = (uint16_t)v;
        esp_err_t err = resync_save((uint16_t)v);
        if (err != ESP_OK) {
            printf("Warning: failed to save to NVS (%s)\n", esp_err_to_name(err));
        }
        post(MSG_RECONFIG, 0, portMAX_DELAY); // wake the task so the new period applies
        printf("OK\n");
        return 0;
    } else if (strcmp(sub, "on") == 0 || strcmp(sub, "off") == 0) {
        post(MSG_RAW_CODE, strcmp(sub, "on") == 0 ? BUTTON_ON : BUTTON_OFF, portMAX_DELAY);
        return 0;
    } else if (strcmp(sub, "defaults") == 0) {
        cfg_set_defaults(&c);
    } else {
        print_usage();
        return 1;
    }

    xSemaphoreTake(s_cfg_lock, portMAX_DELAY);
    s_cfg = c;
    xSemaphoreGive(s_cfg_lock);

    esp_err_t err = cfg_save(&c);
    if (err != ESP_OK) {
        printf("Warning: failed to save to NVS (%s)\n", esp_err_to_name(err));
    }
    post(MSG_RECONFIG, 0, portMAX_DELAY);
    printf("OK\n");
    return 0;
}

void wled_espnow_register_console_cmd(void) {
    const esp_console_cmd_t cmd = {
        .command = "wled",
        .help = "Configure the WLED ESP-NOW link. Run 'wled help' for usage.",
        .hint = "[status|enable|disable|mac|channel|resync|on|off|defaults] ...",
        .func = cmd_wled,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
