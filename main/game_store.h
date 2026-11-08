#ifndef GAME_STORE_H
#define GAME_STORE_H

#include <stdbool.h>
#include "darts_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Restore the last saved game from NVS.
 * @return true if a saved game was loaded, false if none (state untouched).
 */
bool game_store_load(darts_game_state_t *state);

/**
 * @brief Save the game (scores and undo history) to NVS so it survives a reboot
 *        or power cut. Call after every accepted turn, undo or reset.
 */
void game_store_save(const darts_game_state_t *state);

#ifdef __cplusplus
}
#endif

#endif // GAME_STORE_H
