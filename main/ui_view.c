#include "ui_view.h"
#include "app_wifi.h"
#include "bsp_display.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
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

static lv_obj_t *s_scr_obj = NULL;
static lv_obj_t *s_left_panel_obj = NULL;
static lv_obj_t *s_right_panel_obj = NULL;
static lv_obj_t *s_keypad_obj = NULL;

static lv_obj_t *s_score_label = NULL;
static lv_obj_t *s_input_label = NULL;
static lv_obj_t *s_start_over_btn = NULL;
static lv_obj_t *s_start_over_label = NULL;
static lv_obj_t *s_outs_container = NULL;

static lv_obj_t *s_modal_overlay = NULL;
static lv_obj_t *s_menu_panel_obj = NULL;
static lv_obj_t *s_theme_toggle_lbl = NULL;
static lv_obj_t *s_theme_btn_label = NULL;

static lv_obj_t *s_wifi_modal_overlay = NULL;
static lv_obj_t *s_wifi_card = NULL;
static lv_obj_t *s_ssid_ta = NULL;
static lv_obj_t *s_pass_ta = NULL;
static lv_obj_t *s_kb_obj = NULL;
static lv_obj_t *s_wifi_status_label = NULL;

static lv_obj_t *s_out_rows[MAX_OUT_ROWS] = {NULL};
static lv_obj_t *s_out_badge_boxes[MAX_OUT_ROWS][MAX_BADGES_PER_ROW] = {{NULL}};
static lv_obj_t *s_out_badges[MAX_OUT_ROWS][MAX_BADGES_PER_ROW] = {{NULL}};

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

static void close_settings_modal(void) {
  if (s_menu_panel_obj && s_keypad_obj) {
    lv_obj_add_flag(s_menu_panel_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_keypad_obj, LV_OBJ_FLAG_HIDDEN);
  }
}

static void toggle_settings_menu(void) {
  if (s_menu_panel_obj && s_keypad_obj) {
    if (lv_obj_has_flag(s_menu_panel_obj, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_add_flag(s_keypad_obj, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(s_menu_panel_obj, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(s_menu_panel_obj, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(s_keypad_obj, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void on_score_clicked(lv_event_t *e) {
  toggle_settings_menu();
}

static void on_start_over_clicked(lv_event_t *e) {
  if (!s_game_state)
    return;
  s_input_buffer = 0;
  s_input_digits = 0;
  darts_engine_reset(s_game_state);
  ui_view_update();
}

static void on_modal_start_over_clicked(lv_event_t *e) {
  close_settings_modal();
  on_start_over_clicked(e);
}

static void on_modal_undo_clicked(lv_event_t *e) {
  if (!s_game_state)
    return;
  close_settings_modal();
  if (darts_engine_undo_turn(s_game_state)) {
    s_input_buffer = 0;
    s_input_digits = 0;
    ui_view_update();
  }
}

static void close_wifi_settings_modal(void) {
  if (s_wifi_modal_overlay) {
    if (s_scr_obj && !s_modal_overlay) {
      lv_screen_load(s_scr_obj);
    }
    lv_obj_delete_async(s_wifi_modal_overlay);
    s_wifi_modal_overlay = NULL;
    s_wifi_card = NULL;
    s_ssid_ta = NULL;
    s_pass_ta = NULL;
    s_kb_obj = NULL;
    s_wifi_status_label = NULL;
  }
}

static void on_wifi_modal_backdrop_clicked(lv_event_t *e) {
  lv_obj_t *target = lv_event_get_target(e);
  if (target == s_wifi_modal_overlay) {
    if (s_kb_obj && !lv_obj_has_flag(s_kb_obj, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_add_flag(s_kb_obj, LV_OBJ_FLAG_HIDDEN);
      if (s_wifi_card) {
        lv_obj_align(s_wifi_card, LV_ALIGN_CENTER, 0, -20);
      }
    } else {
      close_wifi_settings_modal();
    }
  }
}

static void on_kb_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    if (s_kb_obj) {
      lv_obj_add_flag(s_kb_obj, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_wifi_card) {
      lv_obj_align(s_wifi_card, LV_ALIGN_CENTER, 0, -20);
    }
  }
}

static void on_ta_focus_cb(lv_event_t *e) {
  lv_obj_t *ta = lv_event_get_target(e);
  if (s_kb_obj && ta) {
    lv_keyboard_set_textarea(s_kb_obj, ta);
    lv_obj_clear_flag(s_kb_obj, LV_OBJ_FLAG_HIDDEN);
    if (s_wifi_card) {
      lv_obj_align(s_wifi_card, LV_ALIGN_TOP_MID, 0, 10);
    }
  }
}

static void on_wifi_connect_clicked(lv_event_t *e) {
  if (!s_ssid_ta)
    return;
  const char *ssid = lv_textarea_get_text(s_ssid_ta);
  const char *pass = s_pass_ta ? lv_textarea_get_text(s_pass_ta) : "";

  if (strlen(ssid) > 0) {
    app_wifi_connect(ssid, pass);
    if (s_wifi_status_label) {
      lv_label_set_text(s_wifi_status_label, "Connecting to Wi-Fi...");
    }
    if (s_kb_obj) {
      lv_obj_add_flag(s_kb_obj, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_wifi_card) {
      lv_obj_align(s_wifi_card, LV_ALIGN_CENTER, 0, -20);
    }
  }
}

static void open_wifi_settings_modal(void) {
  if (s_wifi_modal_overlay)
    return;

  // Dedicated Screen View for Wi-Fi Settings (Unloads main view s_scr_obj while
  // active)
  s_wifi_modal_overlay = lv_obj_create(NULL);
  lv_obj_set_size(s_wifi_modal_overlay, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_color(s_wifi_modal_overlay, UI_COLOR_BG, 0);
  lv_obj_set_style_bg_opa(s_wifi_modal_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_wifi_modal_overlay, 0, 0);
  lv_obj_set_style_pad_all(s_wifi_modal_overlay, 0, 0);
  lv_obj_add_flag(s_wifi_modal_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_wifi_modal_overlay, on_wifi_modal_backdrop_clicked,
                      LV_EVENT_CLICKED, NULL);

  // Dialog Box Card (Width 440px, Height 340px)
  s_wifi_card = lv_obj_create(s_wifi_modal_overlay);
  lv_obj_remove_flag(s_wifi_card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(s_wifi_card, 440, 340);
  lv_obj_align(s_wifi_card, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_style_bg_color(s_wifi_card, UI_COLOR_CARD_BG, 0);
  lv_obj_set_style_bg_opa(s_wifi_card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(s_wifi_card, 16, 0);
  lv_obj_set_style_border_width(s_wifi_card, 1, 0);
  lv_obj_set_style_border_color(s_wifi_card, UI_COLOR_DIVIDER, 0);
  lv_obj_set_style_pad_all(s_wifi_card, 16, 0);
  lv_obj_set_flex_flow(s_wifi_card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_wifi_card, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // Header Title Row
  lv_obj_t *header_row = lv_obj_create(s_wifi_card);
  lv_obj_remove_style_all(header_row);
  lv_obj_remove_flag(header_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(header_row, lv_pct(100), 40);
  lv_obj_set_flex_flow(header_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *title = lv_label_create(header_row);
  lv_label_set_text(title, "Wi-Fi Settings");
  lv_obj_set_style_text_font(title, UI_FONT_LARGE, 0);
  lv_obj_set_style_text_color(title, UI_COLOR_TEXT_MAIN, 0);

  lv_obj_t *close_btn = lv_button_create(header_row);
  lv_obj_set_size(close_btn, 34, 34);
  lv_obj_set_style_bg_color(close_btn, UI_COLOR_BTN_BG, 0);
  lv_obj_set_style_radius(close_btn, 17, 0);
  lv_obj_set_style_border_width(close_btn, 0, 0);
  lv_obj_set_style_shadow_width(close_btn, 0, 0);
  lv_obj_add_event_cb(close_btn, (lv_event_cb_t)close_wifi_settings_modal,
                      LV_EVENT_CLICKED, NULL);

  lv_obj_t *close_lbl = lv_label_create(close_btn);
  lv_label_set_text(close_lbl, "X");
  lv_obj_set_style_text_font(close_lbl, UI_FONT_MEDIUM, 0);
  lv_obj_set_style_text_color(close_lbl, UI_COLOR_TEXT_MUTED, 0);
  lv_obj_center(close_lbl);

  // Status Label
  s_wifi_status_label = lv_label_create(s_wifi_card);
  char status_buf[64];
  if (app_wifi_get_state() == APP_WIFI_STATE_CONNECTED) {
    snprintf(status_buf, sizeof(status_buf), "IP: %s (OTA Active)",
             app_wifi_get_ip_str());
    lv_obj_set_style_text_color(s_wifi_status_label, UI_COLOR_SUBMIT, 0);
  } else {
    snprintf(status_buf, sizeof(status_buf), "Status: %s",
             app_wifi_get_ip_str());
    lv_obj_set_style_text_color(s_wifi_status_label, UI_COLOR_TEXT_MUTED, 0);
  }
  lv_label_set_text(s_wifi_status_label, status_buf);
  lv_obj_set_style_text_font(s_wifi_status_label, UI_FONT_OUT, 0);

  // SSID Text Area
  s_ssid_ta = lv_textarea_create(s_wifi_card);
  lv_obj_set_size(s_ssid_ta, lv_pct(100), 42);
  lv_textarea_set_placeholder_text(s_ssid_ta, "Network SSID");
  lv_textarea_set_one_line(s_ssid_ta, true);
  if (strlen(app_wifi_get_ssid()) > 0) {
    lv_textarea_set_text(s_ssid_ta, app_wifi_get_ssid());
  }
  lv_obj_set_style_text_font(s_ssid_ta, UI_FONT_OUT, 0);
  lv_obj_add_event_cb(s_ssid_ta, on_ta_focus_cb, LV_EVENT_FOCUSED, NULL);

  // Password Text Area
  s_pass_ta = lv_textarea_create(s_wifi_card);
  lv_obj_set_size(s_pass_ta, lv_pct(100), 42);
  lv_textarea_set_placeholder_text(s_pass_ta, "Password");
  lv_textarea_set_password_mode(s_pass_ta, true);
  lv_textarea_set_one_line(s_pass_ta, true);
  lv_obj_set_style_text_font(s_pass_ta, UI_FONT_OUT, 0);
  lv_obj_add_event_cb(s_pass_ta, on_ta_focus_cb, LV_EVENT_FOCUSED, NULL);

  // Connect & Save Button
  lv_obj_t *connect_btn = lv_button_create(s_wifi_card);
  lv_obj_set_size(connect_btn, lv_pct(100), 46);
  lv_obj_set_style_bg_color(connect_btn, UI_COLOR_PRIMARY, 0);
  lv_obj_set_style_radius(connect_btn, 10, 0);
  lv_obj_set_style_shadow_width(connect_btn, 0, 0);
  lv_obj_add_event_cb(connect_btn, on_wifi_connect_clicked, LV_EVENT_CLICKED,
                      NULL);

  lv_obj_t *conn_lbl = lv_label_create(connect_btn);
  lv_label_set_text(conn_lbl, "Connect & Save");
  lv_obj_set_style_text_font(conn_lbl, UI_FONT_OUT, 0);
  lv_obj_set_style_text_color(conn_lbl, lv_color_white(), 0);
  lv_obj_center(conn_lbl);

  // Virtual On-Screen QWERTY Keyboard
  s_kb_obj = lv_keyboard_create(s_wifi_modal_overlay);
  lv_obj_set_size(s_kb_obj, lv_pct(100), 220);
  lv_obj_align(s_kb_obj, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(s_kb_obj, s_ssid_ta);
  lv_obj_add_flag(s_kb_obj, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(s_kb_obj, on_kb_event_cb, LV_EVENT_ALL, NULL);

  lv_screen_load(s_wifi_modal_overlay);
}

// static void on_modal_wifi_clicked(lv_event_t *e) {
//   close_settings_modal();
//   open_wifi_settings_modal();
// }

static void open_settings_modal(void);

static void ui_view_refresh_theme(void) {
  if (s_scr_obj) {
    lv_obj_set_style_bg_color(s_scr_obj, UI_COLOR_BG, 0);
  }
  if (s_left_panel_obj) {
    lv_obj_set_style_bg_color(s_left_panel_obj, UI_COLOR_CARD_BG, 0);
  }
  if (s_right_panel_obj) {
    lv_obj_set_style_bg_color(s_right_panel_obj, UI_COLOR_BG, 0);
  }
  if (s_keypad_obj) {
    lv_obj_set_style_bg_color(s_keypad_obj, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_color(s_keypad_obj, UI_COLOR_BTN_BG, LV_PART_ITEMS);
    lv_obj_set_style_text_color(s_keypad_obj, UI_COLOR_TEXT_MAIN,
                                LV_PART_ITEMS);
  }
  if (s_score_label) {
    lv_obj_set_style_text_color(s_score_label, UI_COLOR_TEXT_MAIN, 0);
  }

  // Refresh alternating row colors for outs display
  for (uint8_t i = 0; i < MAX_OUT_ROWS; i++) {
    if (s_out_rows[i]) {
      lv_color_t row_bg =
          (i % 2 == 0) ? UI_COLOR_OUT_ROW_EVEN : UI_COLOR_OUT_ROW_ODD;
      lv_obj_set_style_bg_color(s_out_rows[i], row_bg, 0);
    }
  }

  if (s_modal_overlay) {
    close_settings_modal();
    open_settings_modal();
  } else {
    ui_view_update();
  }
}

// static void on_theme_toggle_clicked(lv_event_t *e) {
//   ui_theme_mode_t cur = ui_theme_get_mode();
//   ui_theme_mode_t next =
//       (cur == UI_THEME_DARK) ? UI_THEME_LIGHT : UI_THEME_DARK;
//   ui_theme_set_mode(next);
//   ui_view_refresh_theme();
// }

static void on_score_long_pressed(lv_event_t *e) { toggle_settings_menu(); }

static void update_outs_display(void) {
  if (!s_outs_container || !s_game_state)
    return;

  bool show_outs =
      (!s_game_state->is_leg_finished && s_game_state->current_score <= 170 &&
       s_game_state->outs_count > 0);
  ESP_LOGI(TAG, "update_outs_display: score=%ld, show_outs=%d, count=%d",
           (long)s_game_state->current_score, show_outs,
           s_game_state->outs_count);

  if (!show_outs) {
    if (!lv_obj_has_flag(s_outs_container, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_add_flag(s_outs_container, LV_OBJ_FLAG_HIDDEN);
      for (uint8_t i = 0; i < MAX_OUT_ROWS; i++) {
        if (s_out_rows[i]) {
          lv_obj_add_flag(s_out_rows[i], LV_OBJ_FLAG_HIDDEN);
        }
      }
    }
    return;
  }

  lv_obj_clear_flag(s_outs_container, LV_OBJ_FLAG_HIDDEN);

  for (uint8_t i = 0; i < MAX_OUT_ROWS; i++) {
    if (i < s_game_state->outs_count) {
      lv_obj_clear_flag(s_out_rows[i], LV_OBJ_FLAG_HIDDEN);

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
    }
  }
}

void ui_view_update(void) {
  esp_task_wdt_reset();
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
    if (s_start_over_btn) {
      lv_obj_add_flag(s_start_over_btn, LV_OBJ_FLAG_HIDDEN);
    }
  } else if (s_game_state->is_leg_finished) {
    snprintf(input_str, sizeof(input_str), "WINNER!");
    lv_obj_set_style_text_color(s_input_label, UI_COLOR_SUBMIT, 0);
    if (s_start_over_btn) {
      lv_obj_clear_flag(s_start_over_btn, LV_OBJ_FLAG_HIDDEN);
    }
  } else {
    if (s_start_over_btn) {
      lv_obj_add_flag(s_start_over_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_input_digits > 0) {
      snprintf(input_str, sizeof(input_str), "TURN: %ld", (long)s_input_buffer);
      lv_obj_set_style_text_color(s_input_label, UI_COLOR_PRIMARY, 0);
    } else {
      snprintf(input_str, sizeof(input_str), "TURN: --");
      lv_obj_set_style_text_color(s_input_label, UI_COLOR_PRIMARY, 0);
    }
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

void ui_view_clear_input(void) {
  s_input_buffer = 0;
  s_input_digits = 0;
  if (s_game_state) {
    s_game_state->is_busted = false;
  }
  ui_view_update();
}

static void on_clear_pressed(void) { ui_view_clear_input(); }

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

  s_scr_obj = lv_screen_active();
  lv_obj_set_style_bg_color(s_scr_obj, UI_COLOR_BG, 0);

  // Main Flex Layout (Horizontal split)
  lv_obj_t *main_container = lv_obj_create(s_scr_obj);
  lv_obj_remove_flag(main_container,
                     LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_size(main_container, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_color(main_container, UI_COLOR_BG, 0);
  lv_obj_set_style_border_width(main_container, 0, 0);
  lv_obj_set_style_pad_all(main_container, 0, 0);
  lv_obj_set_flex_flow(main_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(main_container, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // LEFT PANEL (50% Width): Score, status, and outs
  s_left_panel_obj = lv_obj_create(main_container);
  lv_obj_remove_flag(s_left_panel_obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_left_panel_obj, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_flex_grow(s_left_panel_obj, 1);
  lv_obj_set_height(s_left_panel_obj, lv_pct(100));
  lv_obj_set_style_bg_color(s_left_panel_obj, UI_COLOR_CARD_BG, 0);
  lv_obj_set_style_radius(s_left_panel_obj, 12, 0);
  lv_obj_set_style_border_width(s_left_panel_obj, 0, 0);
  lv_obj_set_style_shadow_width(s_left_panel_obj, 0, 0);
  lv_obj_set_style_pad_hor(s_left_panel_obj, 8, 0);
  lv_obj_set_style_pad_top(s_left_panel_obj, 0, 0);
  lv_obj_set_style_pad_bottom(s_left_panel_obj, 8, 0);
  lv_obj_set_flex_flow(s_left_panel_obj, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_left_panel_obj, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_left_panel_obj, 6, 0);
  lv_obj_add_flag(s_left_panel_obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_left_panel_obj, on_score_clicked, LV_EVENT_CLICKED, NULL);

  // Main Score Display (Native Digits-Only 180pt Segoe UI Font)
  s_score_label = lv_label_create(s_left_panel_obj);
  lv_label_set_long_mode(s_score_label, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_height(s_score_label, 241);
  lv_obj_set_style_margin_top(s_score_label, -36, 0);
  lv_obj_set_style_text_font(s_score_label, UI_FONT_SCORE, 0);
  lv_obj_set_style_text_color(s_score_label, UI_COLOR_TEXT_MAIN, 0);
  lv_obj_set_style_text_align(s_score_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_pad_all(s_score_label, 0, 0);
  lv_obj_add_flag(s_score_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_score_label, on_score_clicked, LV_EVENT_CLICKED, NULL);

  // Active Turn Input Display (50% Larger 54pt Font)
  s_input_label = lv_label_create(s_left_panel_obj);
  lv_label_set_long_mode(s_input_label, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_height(s_input_label, 60);
  lv_obj_set_style_text_font(s_input_label, UI_FONT_INPUT, 0);
  lv_obj_set_style_text_color(s_input_label, UI_COLOR_PRIMARY, 0);

  // "Start Over" Button (Appears under WINNER! when leg finishes)
  s_start_over_btn = lv_button_create(s_left_panel_obj);
  lv_obj_set_size(s_start_over_btn, 280, 70);
  lv_obj_set_style_margin_top(s_start_over_btn, 10, 0);
  lv_obj_set_style_bg_color(s_start_over_btn, UI_COLOR_SUBMIT, 0);
  lv_obj_set_style_bg_opa(s_start_over_btn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(s_start_over_btn, 12, 0);
  lv_obj_set_style_shadow_width(s_start_over_btn, 0, 0);
  lv_obj_add_event_cb(s_start_over_btn, on_start_over_clicked, LV_EVENT_CLICKED,
                      NULL);
  lv_obj_add_flag(s_start_over_btn, LV_OBJ_FLAG_HIDDEN);

  s_start_over_label = lv_label_create(s_start_over_btn);
  lv_label_set_text(s_start_over_label, "Start Over");
  lv_obj_set_style_text_font(s_start_over_label, UI_FONT_INPUT, 0);
  lv_obj_set_style_text_color(s_start_over_label, lv_color_white(), 0);
  lv_obj_center(s_start_over_label);

  // Outs Container (Max 2 rows, flex column flow)
  s_outs_container = lv_obj_create(s_left_panel_obj);
  lv_obj_remove_flag(s_outs_container,
                     LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_width(s_outs_container, lv_pct(100));
  lv_obj_set_height(s_outs_container, 120);
  lv_obj_set_style_bg_opa(s_outs_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_outs_container, 0, 0);
  lv_obj_set_style_pad_all(s_outs_container, 0, 0);
  lv_obj_set_style_margin_top(s_outs_container, 6, 0);
  lv_obj_set_flex_flow(s_outs_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_outs_container, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_outs_container, 6, 0);

  // Pre-create 3 out rows with alternating background colors and pill badge
  // labels
  for (uint8_t i = 0; i < MAX_OUT_ROWS; i++) {
    s_out_rows[i] = lv_obj_create(s_outs_container);
    lv_obj_remove_flag(s_out_rows[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(s_out_rows[i], lv_pct(100));
    lv_obj_set_height(s_out_rows[i], 52);

    lv_color_t row_bg =
        (i % 2 == 0) ? UI_COLOR_OUT_ROW_EVEN : UI_COLOR_OUT_ROW_ODD;
    lv_obj_set_style_bg_color(s_out_rows[i], row_bg, 0);
    lv_obj_set_style_bg_opa(s_out_rows[i], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_out_rows[i], 8, 0);
    lv_obj_set_style_border_width(s_out_rows[i], 0, 0);
    lv_obj_set_style_pad_ver(s_out_rows[i], 3, 0);
    lv_obj_set_style_pad_hor(s_out_rows[i], 6, 0);
    lv_obj_set_flex_flow(s_out_rows[i], LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_out_rows[i], LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_out_rows[i], 10, 0);
    lv_obj_add_flag(s_out_rows[i], LV_OBJ_FLAG_HIDDEN);

    for (uint8_t b = 0; b < MAX_BADGES_PER_ROW; b++) {
      s_out_badge_boxes[i][b] = lv_obj_create(s_out_rows[i]);
      lv_obj_remove_flag(s_out_badge_boxes[i][b], LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_width(s_out_badge_boxes[i][b], 100);
      lv_obj_set_height(s_out_badge_boxes[i][b], 46);
      lv_obj_set_style_radius(s_out_badge_boxes[i][b], 8, 0);
      lv_obj_set_style_border_width(s_out_badge_boxes[i][b], 0, 0);
      lv_obj_set_style_pad_ver(s_out_badge_boxes[i][b], 2, 0);
      lv_obj_set_style_pad_hor(s_out_badge_boxes[i][b], 8, 0);
      lv_obj_set_style_bg_opa(s_out_badge_boxes[i][b], LV_OPA_COVER, 0);
      lv_obj_add_flag(s_out_badge_boxes[i][b], LV_OBJ_FLAG_HIDDEN);

      s_out_badges[i][b] = lv_label_create(s_out_badge_boxes[i][b]);
      lv_label_set_long_mode(s_out_badges[i][b], LV_LABEL_LONG_MODE_CLIP);
      lv_obj_set_style_text_font(s_out_badges[i][b], UI_FONT_OUT, 0);
      lv_obj_center(s_out_badges[i][b]);
    }
  }

  // RIGHT PANEL (50% Width): Keypad Control
  s_right_panel_obj = lv_obj_create(main_container);
  lv_obj_remove_flag(s_right_panel_obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_grow(s_right_panel_obj, 1);
  lv_obj_set_height(s_right_panel_obj, lv_pct(100));
  lv_obj_set_style_bg_color(s_right_panel_obj, UI_COLOR_BG, 0);
  lv_obj_set_style_border_width(s_right_panel_obj, 0, 0);
  lv_obj_set_style_pad_all(s_right_panel_obj, 0, 0);

  ui_keypad_callbacks_t keypad_cbs = {.on_digit = on_digit_pressed,
                                      .on_clear = on_clear_pressed,
                                      .on_submit = on_submit_pressed};
  s_keypad_obj = ui_keypad_create(s_right_panel_obj, keypad_cbs);

  // Settings Menu Panel (swaps with keypad in right panel when gear icon is tapped)
  s_menu_panel_obj = lv_obj_create(s_right_panel_obj);
  lv_obj_remove_flag(s_menu_panel_obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(s_menu_panel_obj, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_color(s_menu_panel_obj, UI_COLOR_BG, 0);
  lv_obj_set_style_border_width(s_menu_panel_obj, 0, 0);
  lv_obj_set_style_pad_all(s_menu_panel_obj, 10, 0);
  lv_obj_set_flex_flow(s_menu_panel_obj, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_menu_panel_obj, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(s_menu_panel_obj, LV_OBJ_FLAG_HIDDEN);

  // Button 1: Undo Last Turn (190px height)
  lv_obj_t *undo_btn = lv_button_create(s_menu_panel_obj);
  lv_obj_set_size(undo_btn, lv_pct(100), 195);
  lv_obj_set_style_bg_color(undo_btn, lv_color_hex(0xD97706), 0);
  lv_obj_set_style_radius(undo_btn, 14, 0);
  lv_obj_set_style_shadow_width(undo_btn, 0, 0);
  lv_obj_add_event_cb(undo_btn, on_modal_undo_clicked, LV_EVENT_CLICKED, NULL);

  lv_obj_t *undo_lbl = lv_label_create(undo_btn);
  lv_label_set_text(undo_lbl, "Undo Last Turn");
  lv_obj_set_style_text_font(undo_lbl, UI_FONT_INPUT, 0);
  lv_obj_set_style_text_color(undo_lbl, lv_color_white(), 0);
  lv_obj_center(undo_lbl);

  // Button 2: Start Over (New Game - 190px height)
  lv_obj_t *reset_btn = lv_button_create(s_menu_panel_obj);
  lv_obj_set_size(reset_btn, lv_pct(100), 195);
  lv_obj_set_style_bg_color(reset_btn, UI_COLOR_CLEAR, 0);
  lv_obj_set_style_radius(reset_btn, 14, 0);
  lv_obj_set_style_shadow_width(reset_btn, 0, 0);
  lv_obj_add_event_cb(reset_btn, on_modal_start_over_clicked, LV_EVENT_CLICKED,
                      NULL);

  lv_obj_t *reset_lbl = lv_label_create(reset_btn);
  lv_label_set_text(reset_lbl, "Start Over");
  lv_obj_set_style_text_font(reset_lbl, UI_FONT_INPUT, 0);
  lv_obj_set_style_text_color(reset_lbl, lv_color_white(), 0);
  lv_obj_center(reset_lbl);

  // Initial UI render
  ui_view_update();
}

void ui_view_prepare_for_ota(void) {
  bsp_display_lock();
  if (s_scr_obj) {
    lv_obj_set_style_bg_color(s_scr_obj, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_scr_obj, LV_OPA_COVER, 0);
  }
  if (s_left_panel_obj) {
    lv_obj_add_flag(s_left_panel_obj, LV_OBJ_FLAG_HIDDEN);
  }
  if (s_right_panel_obj) {
    lv_obj_add_flag(s_right_panel_obj, LV_OBJ_FLAG_HIDDEN);
  }
  if (s_modal_overlay) {
    close_settings_modal();
  }
  if (s_wifi_modal_overlay) {
    close_wifi_settings_modal();
  }
  lv_refr_now(NULL);
  bsp_display_unlock();

  bsp_display_pause_for_ota();
}

void ui_view_sleep(void) {
  ui_view_clear_input();
  if (s_scr_obj) {
    lv_obj_set_style_bg_color(s_scr_obj, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_scr_obj, LV_OPA_COVER, 0);
  }
  if (s_left_panel_obj) {
    lv_obj_add_flag(s_left_panel_obj, LV_OBJ_FLAG_HIDDEN);
  }
  if (s_right_panel_obj) {
    lv_obj_add_flag(s_right_panel_obj, LV_OBJ_FLAG_HIDDEN);
  }
  if (s_modal_overlay) {
    close_settings_modal();
  }
  if (s_wifi_modal_overlay) {
    close_wifi_settings_modal();
  }
  lv_refr_now(NULL);
}

void ui_view_wake(void) {
  if (s_scr_obj) {
    lv_obj_set_style_bg_color(s_scr_obj, UI_COLOR_BG, 0);
  }
  if (s_left_panel_obj) {
    lv_obj_clear_flag(s_left_panel_obj, LV_OBJ_FLAG_HIDDEN);
  }
  if (s_right_panel_obj) {
    lv_obj_clear_flag(s_right_panel_obj, LV_OBJ_FLAG_HIDDEN);
  }
  ui_view_update();
}
