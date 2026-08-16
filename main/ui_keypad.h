#ifndef UI_KEYPAD_H
#define UI_KEYPAD_H

#include "lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_keypad_digit_cb_t)(uint8_t digit);
typedef void (*ui_keypad_action_cb_t)(void);

typedef struct {
    ui_keypad_digit_cb_t on_digit;
    ui_keypad_action_cb_t on_clear;
    ui_keypad_action_cb_t on_submit;
} ui_keypad_callbacks_t;

/**
 * @brief Create custom numeric keypad container in parent object.
 * @param parent Parent LVGL container object.
 * @param callbacks Event callbacks for digit, clear, and submit presses.
 * @return Pointer to created keypad container object.
 */
lv_obj_t *ui_keypad_create(lv_obj_t *parent, ui_keypad_callbacks_t callbacks);

#ifdef __cplusplus
}
#endif

#endif // UI_KEYPAD_H
