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

/**
 * @brief Clear active turn input buffer and refresh input display.
 */
void ui_view_clear_input(void);

/**
 * @brief Fill screen with pure black and flush before OTA flashing.
 */
void ui_view_prepare_for_ota(void);

/**
 * @brief Put UI to sleep (clear input, fill black, hide panels).
 */
void ui_view_sleep(void);

/**
 * @brief Restore UI on wake from sleep.
 */
void ui_view_wake(void);

#ifdef __cplusplus
}
#endif

#endif // UI_VIEW_H
