#ifndef DARTS_ENGINE_H
#define DARTS_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_OUT_COMBINATIONS  3
#define MAX_OUT_STR_LEN       64
#define DEFAULT_START_SCORE   301

typedef struct {
    char str[MAX_OUT_STR_LEN];
    uint8_t darts_count;
} checkout_option_t;

typedef struct {
    int32_t current_score;
    int32_t starting_score;
    int32_t last_turn_score;
    bool is_busted;
    bool is_leg_finished;
    checkout_option_t outs[MAX_OUT_COMBINATIONS];
    uint8_t outs_count;
} darts_game_state_t;

/**
 * @brief Initialize game state with specified starting score (default 301).
 */
void darts_engine_init(darts_game_state_t *state, int32_t start_score);

/**
 * @brief Submit turn score achieved by player.
 * @return true if valid score accepted (including leg win), false if bust/invalid.
 */
bool darts_engine_submit_turn(darts_game_state_t *state, int32_t turn_score);

/**
 * @brief Reset engine to starting state.
 */
void darts_engine_reset(darts_game_state_t *state);

/**
 * @brief Calculate outs/checkouts for a given score.
 */
uint8_t darts_engine_get_checkouts(int32_t score, checkout_option_t outs[MAX_OUT_COMBINATIONS]);

#ifdef __cplusplus
}
#endif

#endif // DARTS_ENGINE_H
