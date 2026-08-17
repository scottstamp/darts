#include "ui_view.h"
#include "esp_log.h"
#include "ui_keypad.h"
#include "ui_theme.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "ui_view";

#define MAX_OUT_ROWS 2
#define MAX_BADGES_PER_ROW 3

static darts_game_state_t *s_game_state = NULL;

static int32_t s_input_buffer = 0;
static uint8_t s_input_digits = 0;

static lv_obj_t *s_score_label = NULL;
static lv_obj_t *s_input_label = NULL;
static lv_obj_t *s_outs_container = NULL;

static lv_obj_t *s_out_rows[MAX_OUT_ROWS] = {NULL};
static lv_obj_t *s_out_badge_boxes[MAX_OUT_ROWS][MAX_BADGES_PER_ROW] = {{NULL}};
static lv_obj_t *s_out_badges[MAX_OUT_ROWS][MAX_BADGES_PER_ROW] = {{NULL}};
static lv_obj_t *s_out_dividers[MAX_OUT_ROWS - 1] = {NULL};

static void trim_str(char *s) {
  if (!s || *s == '\0')
    return;
  char *p = s;
  while (*p == ' ')
    p++;
  if (p != s) {
    memmove(s, p, strlen(p) + 1);
  }
  size_t len = strlen(s);
  while (len > 0 && s[len - 1] == ' ') {
    s[len - 1] = '\0';
    len--;
  }
}

static void update_outs_display(void) {
  if (!s_outs_container || !s_game_state)
    return;

  bool show_outs =
      (!s_game_state->is_leg_finished && s_game_state->current_score <= 170 &&
       s_game_state->outs_count > 0);
  ESP_LOGI(TAG, "update_outs_display: score=%ld, show_outs=%d, count=%d",
           (long)s_game_state->current_score, show_outs,
           s_game_state->outs_count);

  for (uint8_t i = 0; i < MAX_OUT_ROWS; i++) {
    if (show_outs && i < s_game_state->outs_count) {
      lv_obj_clear_flag(s_out_rows[i], LV_OBJ_FLAG_HIDDEN);
      if (i > 0) {
        lv_obj_clear_flag(s_out_dividers[i - 1], LV_OBJ_FLAG_HIDDEN);
      }

      char str_copy[MAX_OUT_STR_LEN];
      strncpy(str_copy, s_game_state->outs[i].str, sizeof(str_copy) - 1);
      str_copy[sizeof(str_copy) - 1] = '\0';

      char *saveptr = NULL;
      char *token = strtok_r(str_copy, "-", &saveptr);
      uint8_t badge_idx = 0;

      while (token != NULL && badge_idx < MAX_BADGES_PER_ROW) {
        trim_str(token);

        if (strlen(token) > 0) {
          lv_obj_clear_flag(s_out_badge_boxes[i][badge_idx],
                            LV_OBJ_FLAG_HIDDEN);
          lv_label_set_text(s_out_badges[i][badge_idx], token);

          if (i == 0) {
            lv_obj_set_style_bg_color(s_out_badge_boxes[i][badge_idx],
                                      UI_COLOR_PRIMARY, 0);
            lv_obj_set_style_text_color(s_out_badges[i][badge_idx],
                                        lv_color_white(), 0);
          } else {
            lv_obj_set_style_bg_color(s_out_badge_boxes[i][badge_idx],
                                      UI_COLOR_BTN_BG, 0);
            lv_obj_set_style_text_color(s_out_badges[i][badge_idx],
                                        UI_COLOR_TEXT_MAIN, 0);
          }
          badge_idx++;
        }
        token = strtok_r(NULL, "-", &saveptr);
      }

      for (uint8_t b = badge_idx; b < MAX_BADGES_PER_ROW; b++) {
        lv_obj_add_flag(s_out_badge_boxes[i][b], LV_OBJ_FLAG_HIDDEN);
      }
    } else {
      lv_obj_add_flag(s_out_rows[i], LV_OBJ_FLAG_HIDDEN);
      if (i > 0) {
        lv_obj_add_flag(s_out_dividers[i - 1], LV_OBJ_FLAG_HIDDEN);
      }
    }
  }
}

void ui_view_update(void) {
  if (!s_game_state)
    return;

  // Update Score Display
  char score_str[32];
  snprintf(score_str, sizeof(score_str), "%ld",
           (long)s_game_state->current_score);
  lv_label_set_text(s_score_label, score_str);

  // Update Input Display
  char input_str[32];
  if (s_game_state->is_busted) {
    snprintf(input_str, sizeof(input_str), "BUST!");
    lv_obj_set_style_text_color(s_input_label, UI_COLOR_BUST, 0);
  } else if (s_game_state->is_leg_finished) {
    snprintf(input_str, sizeof(input_str), "WINNER!");
    lv_obj_set_style_text_color(s_input_label, UI_COLOR_SUBMIT, 0);
  } else if (s_input_digits > 0) {
    snprintf(input_str, sizeof(input_str), "TURN: %ld", (long)s_input_buffer);
    lv_obj_set_style_text_color(s_input_label, UI_COLOR_PRIMARY, 0);
  } else {
    snprintf(input_str, sizeof(input_str), "TURN: --");
    lv_obj_set_style_text_color(s_input_label, UI_COLOR_PRIMARY, 0);
  }
  lv_label_set_text(s_input_label, input_str);

  // Update Outs list
  update_outs_display();
}

static void on_digit_pressed(uint8_t digit) {
  if (s_game_state && s_game_state->is_leg_finished)
    return;

  // Limit to 3 digits and max score 180
  if (s_input_digits < 3) {
    int32_t temp = s_input_buffer * 10 + digit;
    if (temp <= 180) {
      s_input_buffer = temp;
      s_input_digits++;
      char input_str[32];
      snprintf(input_str, sizeof(input_str), "TURN: %ld", (long)s_input_buffer);
      lv_label_set_text(s_input_label, input_str);
    }
  }
}

static void on_clear_pressed(void) {
  s_input_buffer = 0;
  s_input_digits = 0;
  if (s_game_state) {
    s_game_state->is_busted = false;
  }
  ui_view_update();
}

static void on_submit_pressed(void) {
  if (!s_game_state || s_game_state->is_leg_finished)
    return;

  if (s_input_digits > 0) {
    darts_engine_submit_turn(s_game_state, s_input_buffer);
    s_input_buffer = 0;
    s_input_digits = 0;
    ui_view_update();
  }
}

void ui_view_init(darts_game_state_t *state) {
  s_game_state = state;

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, UI_COLOR_BG, 0);

  // Main Flex Layout (Horizontal split)
  lv_obj_t *main_container = lv_obj_create(scr);
  lv_obj_remove_flag(main_container, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_size(main_container, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_color(main_container, UI_COLOR_BG, 0);
  lv_obj_set_style_border_width(main_container, 0, 0);
  lv_obj_set_style_pad_all(main_container, 0, 0);
  lv_obj_set_flex_flow(main_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(main_container, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // LEFT PANEL (50% Width): Score, status, and outs
  lv_obj_t *left_panel = lv_obj_create(main_container);
  lv_obj_remove_flag(left_panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_flex_grow(left_panel, 1);
  lv_obj_set_height(left_panel, lv_pct(100));
  lv_obj_set_style_bg_color(left_panel, UI_COLOR_CARD_BG, 0);
  lv_obj_set_style_radius(left_panel, 12, 0);
  lv_obj_set_style_border_width(left_panel, 0, 0);
  lv_obj_set_style_shadow_width(left_panel, 4, 0);
  lv_obj_set_style_shadow_color(left_panel, UI_COLOR_CARD_SHADOW, 0);
  lv_obj_set_style_pad_hor(left_panel, 8, 0);
  lv_obj_set_style_pad_top(left_panel, 4, 0);
  lv_obj_set_style_pad_bottom(left_panel, 8, 0);
  lv_obj_set_flex_flow(left_panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(left_panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(left_panel, 6, 0);

  // Main Score Display (Native Digits-Only 140pt Segoe UI Font, 4px from top)
  s_score_label = lv_label_create(left_panel);
  lv_label_set_long_mode(s_score_label, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_height(s_score_label, 175);
  lv_obj_set_style_text_font(s_score_label, UI_FONT_SCORE, 0);
  lv_obj_set_style_text_color(s_score_label, UI_COLOR_TEXT_MAIN, 0);
  lv_obj_set_style_text_align(s_score_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_pad_all(s_score_label, 0, 0);

  // Active Turn Input Display (50% Larger 54pt Font)
  s_input_label = lv_label_create(left_panel);
  lv_label_set_long_mode(s_input_label, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_height(s_input_label, 60);
  lv_obj_set_style_text_font(s_input_label, UI_FONT_INPUT, 0);
  lv_obj_set_style_text_color(s_input_label, UI_COLOR_PRIMARY, 0);

  // Outs Container (Max 2 rows, fixed 140px height)
  s_outs_container = lv_obj_create(left_panel);
  lv_obj_remove_flag(s_outs_container, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_width(s_outs_container, lv_pct(100));
  lv_obj_set_height(s_outs_container, 140);
  lv_obj_set_style_bg_opa(s_outs_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_outs_container, 0, 0);
  lv_obj_set_style_pad_all(s_outs_container, 0, 0);

  // Pre-create out rows and pill badge labels
  for (uint8_t i = 0; i < MAX_OUT_ROWS; i++) {
    s_out_rows[i] = lv_obj_create(s_outs_container);
    lv_obj_remove_flag(s_out_rows[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(s_out_rows[i], lv_pct(100));
    lv_obj_set_height(s_out_rows[i], 36);
    lv_obj_set_y(s_out_rows[i], i == 0 ? 10 : 70);
    lv_obj_set_style_bg_opa(s_out_rows[i], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_out_rows[i], 0, 0);
    lv_obj_set_style_pad_ver(s_out_rows[i], 0, 0);
    lv_obj_set_style_pad_hor(s_out_rows[i], 0, 0);
    lv_obj_set_flex_flow(s_out_rows[i], LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_out_rows[i], LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_out_rows[i], 10, 0);
    lv_obj_add_flag(s_out_rows[i], LV_OBJ_FLAG_HIDDEN);

    for (uint8_t b = 0; b < MAX_BADGES_PER_ROW; b++) {
      s_out_badge_boxes[i][b] = lv_obj_create(s_out_rows[i]);
      lv_obj_remove_flag(s_out_badge_boxes[i][b], LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_width(s_out_badge_boxes[i][b], 72);
      lv_obj_set_height(s_out_badge_boxes[i][b], 32);
      lv_obj_set_style_radius(s_out_badge_boxes[i][b], 8, 0);
      lv_obj_set_style_border_width(s_out_badge_boxes[i][b], 0, 0);
      lv_obj_set_style_pad_ver(s_out_badge_boxes[i][b], 4, 0);
      lv_obj_set_style_pad_hor(s_out_badge_boxes[i][b], 14, 0);
      lv_obj_set_style_bg_opa(s_out_badge_boxes[i][b], LV_OPA_COVER, 0);
      lv_obj_add_flag(s_out_badge_boxes[i][b], LV_OBJ_FLAG_HIDDEN);

      s_out_badges[i][b] = lv_label_create(s_out_badge_boxes[i][b]);
      lv_label_set_long_mode(s_out_badges[i][b], LV_LABEL_LONG_MODE_CLIP);
      lv_obj_set_style_text_font(s_out_badges[i][b], UI_FONT_MEDIUM, 0);
      lv_obj_center(s_out_badges[i][b]);
    }

    if (i < MAX_OUT_ROWS - 1) {
      s_out_dividers[i] = lv_obj_create(s_outs_container);
      lv_obj_remove_flag(s_out_dividers[i], LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_size(s_out_dividers[i], lv_pct(100), 1);
      lv_obj_set_y(s_out_dividers[i], 55);
      lv_obj_set_style_bg_color(s_out_dividers[i], UI_COLOR_DIVIDER, 0);
      lv_obj_set_style_border_width(s_out_dividers[i], 0, 0);
      lv_obj_set_style_pad_all(s_out_dividers[i], 0, 0);
      lv_obj_add_flag(s_out_dividers[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  // RIGHT PANEL (50% Width): Keypad Control
  lv_obj_t *right_panel = lv_obj_create(main_container);
  lv_obj_remove_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_grow(right_panel, 1);
  lv_obj_set_height(right_panel, lv_pct(100));
  lv_obj_set_style_bg_color(right_panel, UI_COLOR_BG, 0);
  lv_obj_set_style_border_width(right_panel, 0, 0);
  lv_obj_set_style_pad_all(right_panel, 0, 0);

  ui_keypad_callbacks_t keypad_cbs = {.on_digit = on_digit_pressed,
                                      .on_clear = on_clear_pressed,
                                      .on_submit = on_submit_pressed};
  ui_keypad_create(right_panel, keypad_cbs);

  // Initial UI render
  ui_view_update();
}
