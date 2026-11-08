#include "game_store.h"
#include <string.h>
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "game_store";

#define NVS_NAMESPACE  "game"
#define NVS_KEY        "state"
#define STORE_VERSION  1

// Compact on-flash form (~140 bytes). Scores fit easily in 16 bits.
typedef struct {
    uint8_t version;
    uint8_t leg_finished;
    uint16_t history_count;
    int16_t starting_score;
    int16_t current_score;
    int16_t last_turn_score;
    int16_t history[MAX_TURN_HISTORY];
} stored_game_t;

bool game_store_load(darts_game_state_t *state) {
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;

    stored_game_t g;
    size_t len = sizeof(g);
    esp_err_t err = nvs_get_blob(h, NVS_KEY, &g, &len);
    nvs_close(h);
    if (err != ESP_OK || len != sizeof(g) || g.version != STORE_VERSION ||
        g.history_count > MAX_TURN_HISTORY || g.starting_score <= 0 ||
        g.current_score < 0 || g.current_score > g.starting_score) {
        return false;
    }

    darts_engine_init(state, g.starting_score);
    state->current_score = g.current_score;
    state->last_turn_score = g.last_turn_score;
    state->is_leg_finished = g.leg_finished;
    state->turn_history_count = g.history_count;
    for (uint16_t i = 0; i < g.history_count; i++) {
        state->score_history[i] = g.history[i];
    }
    state->outs_count = state->is_leg_finished
                            ? 0
                            : darts_engine_get_checkouts(state->current_score, state->outs);
    ESP_LOGI(TAG, "Restored game: %ld of %ld, %u turns", (long)state->current_score,
             (long)state->starting_score, g.history_count);
    return true;
}

void game_store_save(const darts_game_state_t *state) {
    stored_game_t g = {0};
    g.version = STORE_VERSION;
    g.leg_finished = state->is_leg_finished;
    g.history_count = state->turn_history_count;
    g.starting_score = (int16_t)state->starting_score;
    g.current_score = (int16_t)state->current_score;
    g.last_turn_score = (int16_t)state->last_turn_score;
    for (uint16_t i = 0; i < state->turn_history_count && i < MAX_TURN_HISTORY; i++) {
        g.history[i] = (int16_t)state->score_history[i];
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, NVS_KEY, &g, sizeof(g));
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save game: %s", esp_err_to_name(err));
    }
}
