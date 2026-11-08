#include "app_settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bsp_display.h"
#include "esp_console.h"
#include "esp_log.h"
#include "nvs.h"
#include "ui_view.h"

static const char *TAG = "app_settings";

#define NVS_NAMESPACE        "settings"
#define DEFAULT_TIMEOUT_SEC  300
#define DEFAULT_BRIGHTNESS   100
#define MAX_TIMEOUT_SEC      86400
#define MIN_START_SCORE      2
#define MAX_START_SCORE      9999

static esp_err_t save_u32(const char *key, uint32_t val) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u32(h, key, val);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

void app_settings_load(void) {
    uint32_t timeout = DEFAULT_TIMEOUT_SEC;
    uint32_t brightness = DEFAULT_BRIGHTNESS;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u32(h, "timeout", &timeout);
        nvs_get_u32(h, "bright", &brightness);
        nvs_close(h);
    }
    if (timeout > MAX_TIMEOUT_SEC) timeout = DEFAULT_TIMEOUT_SEC;
    if (brightness < 1 || brightness > 100) brightness = DEFAULT_BRIGHTNESS;
    bsp_display_set_timeout(timeout);
    bsp_display_set_brightness((uint8_t)brightness);
    ESP_LOGI(TAG, "Screen timeout %lus, brightness %lu%%", (unsigned long)timeout,
             (unsigned long)brightness);
}

esp_err_t app_settings_set_timeout(uint32_t seconds) {
    if (seconds > MAX_TIMEOUT_SEC) return ESP_ERR_INVALID_ARG;
    bsp_display_set_timeout(seconds);
    return save_u32("timeout", seconds);
}

esp_err_t app_settings_set_brightness(uint8_t pct) {
    if (pct < 1 || pct > 100) return ESP_ERR_INVALID_ARG;
    bsp_display_set_brightness(pct);
    return save_u32("bright", pct);
}

/* ---------------- Serial console ---------------- */

static bool parse_long(const char *s, long min, long max, long *out) {
    char *end;
    long v = strtol(s, &end, 10);
    if (*s == '\0' || *end != '\0' || v < min || v > max) return false;
    *out = v;
    return true;
}

static int cmd_display(int argc, char **argv) {
    long v;
    esp_err_t err;
    if (argc == 1) {
        uint32_t t = bsp_display_get_timeout();
        if (t == 0) {
            printf("Screen timeout: never\n");
        } else {
            printf("Screen timeout: %lu s\n", (unsigned long)t);
        }
        printf("Brightness    : %u %%\n", bsp_display_get_brightness());
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "timeout") == 0 && parse_long(argv[2], 0, MAX_TIMEOUT_SEC, &v)) {
        err = app_settings_set_timeout((uint32_t)v);
    } else if (argc == 3 && strcmp(argv[1], "brightness") == 0 && parse_long(argv[2], 1, 100, &v)) {
        err = app_settings_set_brightness((uint8_t)v);
    } else {
        printf("Usage:\n"
               "  display                     Show settings\n"
               "  display timeout <seconds>   Sleep after inactivity (0 = never, max %d)\n"
               "  display brightness <1-100>  Backlight brightness in %%\n",
               MAX_TIMEOUT_SEC);
        return 1;
    }
    if (err != ESP_OK) {
        printf("Failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    printf("OK\n");
    return 0;
}

static int cmd_game(int argc, char **argv) {
    if (argc == 1) {
        darts_game_state_t s;
        ui_view_get_game_state(&s);
        printf("Game %ld: %ld remaining after %u turns%s\n", (long)s.starting_score,
               (long)s.current_score, s.turn_history_count,
               s.is_leg_finished ? " (finished)" : "");
        return 0;
    }
    if (strcmp(argv[1], "new") == 0 && (argc == 2 || argc == 3)) {
        long start = 0; // 0 = keep current starting score
        if (argc == 3 && !parse_long(argv[2], MIN_START_SCORE, MAX_START_SCORE, &start)) {
            printf("Starting score must be %d-%d\n", MIN_START_SCORE, MAX_START_SCORE);
            return 1;
        }
        bsp_display_lock();
        ui_view_new_game((int32_t)start);
        bsp_display_unlock();
        printf("OK\n");
        return 0;
    }
    printf("Usage:\n"
           "  game                Show current game\n"
           "  game new [score]    Start a new game (e.g. 'game new 501'); the\n"
           "                      starting score is kept for later games\n");
    return 1;
}

void app_settings_register_console_cmds(void) {
    const esp_console_cmd_t display_cmd = {
        .command = "display",
        .help = "Screen timeout and brightness. Run 'display help' for usage.",
        .func = cmd_display,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&display_cmd));

    const esp_console_cmd_t game_cmd = {
        .command = "game",
        .help = "Show the game or start a new one ('game new [score]').",
        .func = cmd_game,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&game_cmd));
}
