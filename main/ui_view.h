#ifndef UI_VIEW_H
#define UI_VIEW_H

#include "lvgl.h"
#include "darts_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and build the complete scoreboard UI layout.
 * @param state Pointer to the active darts game engine state.
 */
void ui_view_init(darts_game_state_t *state);

/**
 * @brief Update UI widgets from current game engine state.
 */
void ui_view_update(void);

#ifdef __cplusplus
}
#endif

#endif // UI_VIEW_H
