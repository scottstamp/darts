#include "darts_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *name;
  uint8_t value;
  bool is_double;
} dart_target_t;

// Standard Darts Targets in preferred setup order (Triples first, then Singles,
// then Doubles for finish)
static const dart_target_t ALL_TARGETS[] = {
    // Triples (High scoring setup)
    {"T20", 60, false},
    {"T19", 57, false},
    {"T18", 54, false},
    {"T17", 51, false},
    {"T16", 48, false},
    {"T15", 45, false},
    {"T14", 42, false},
    {"T13", 39, false},
    {"T12", 36, false},
    {"T11", 33, false},
    {"T10", 30, false},
    {"T9", 27, false},
    {"T8", 24, false},
    {"T7", 21, false},
    {"T6", 18, false},
    {"T5", 15, false},
    {"T4", 12, false},
    {"T3", 9, false},
    {"T2", 6, false},
    {"T1", 3, false},

    // Singles
    {"20", 20, false},
    {"19", 19, false},
    {"18", 18, false},
    {"17", 17, false},
    {"16", 16, false},
    {"15", 15, false},
    {"14", 14, false},
    {"13", 13, false},
    {"12", 12, false},
    {"11", 11, false},
    {"10", 10, false},
    {"9", 9, false},
    {"8", 8, false},
    {"7", 7, false},
    {"6", 6, false},
    {"5", 5, false},
    {"4", 4, false},
    {"3", 3, false},
    {"2", 2, false},
    {"1", 1, false},
    {"25", 25, false},

    // Doubles (Valid Finishes)
    {"D20", 40, true},
    {"D16", 32, true},
    {"D10", 20, true},
    {"D8", 16, true},
    {"D12", 24, true},
    {"D18", 36, true},
    {"D14", 28, true},
    {"BULL", 50, true},
    {"D19", 38, true},
    {"D17", 34, true},
    {"D15", 30, true},
    {"D13", 26, true},
    {"D11", 22, true},
    {"D9", 18, true},
    {"D7", 14, true},
    {"D6", 12, true},
    {"D5", 10, true},
    {"D4", 8, true},
    {"D3", 6, true},
    {"D2", 4, true},
    {"D1", 2, true}};

static const size_t TARGETS_COUNT =
    sizeof(ALL_TARGETS) / sizeof(ALL_TARGETS[0]);

// O(1) Direct lookup for valid double finishes
static const char *get_double_target_name(uint8_t val) {
  switch (val) {
  case 40:
    return "D20";
  case 38:
    return "D19";
  case 36:
    return "D18";
  case 34:
    return "D17";
  case 32:
    return "D16";
  case 30:
    return "D15";
  case 28:
    return "D14";
  case 26:
    return "D13";
  case 24:
    return "D12";
  case 22:
    return "D11";
  case 20:
    return "D10";
  case 18:
    return "D9";
  case 16:
    return "D8";
  case 14:
    return "D7";
  case 12:
    return "D6";
  case 10:
    return "D5";
  case 8:
    return "D4";
  case 6:
    return "D3";
  case 4:
    return "D2";
  case 2:
    return "D1";
  case 50:
    return "BULL"; // Bullseye
  default:
    return NULL;
  }
}

void darts_engine_init(darts_game_state_t *state, int32_t start_score) {
  if (!state)
    return;
  state->starting_score = (start_score > 0) ? start_score : DEFAULT_START_SCORE;
  darts_engine_reset(state);
}

void darts_engine_reset(darts_game_state_t *state) {
  if (!state)
    return;
  state->current_score = state->starting_score;
  state->last_turn_score = 0;
  state->is_busted = false;
  state->is_leg_finished = false;
  state->outs_count =
      darts_engine_get_checkouts(state->current_score, state->outs);
}

uint8_t
darts_engine_get_checkouts(int32_t score,
                           checkout_option_t outs[MAX_OUT_COMBINATIONS]) {
  // Filter impossible checkouts immediately (score < 2, score > 170, or
  // impossible 3-dart checkout numbers)
  if (score < 2 || score > 170 || score == 169 || score == 166 ||
      score == 165 || score == 163 || score == 162 || score == 159) {
    return 0;
  }

  uint8_t count = 0;

  // 1-Dart Outs (Must be Double)
  const char *d1_name = get_double_target_name(score);
  if (d1_name != NULL) {
    snprintf(outs[count].str, MAX_OUT_STR_LEN, "%s", d1_name);
    outs[count].darts_count = 1;
    count++;
  }

  // 2-Dart Outs (Dart 1: Any, Dart 2: Double)
  if (count < MAX_OUT_COMBINATIONS) {
    for (size_t i = 0; i < TARGETS_COUNT && count < MAX_OUT_COMBINATIONS; i++) {
      int32_t rem = score - ALL_TARGETS[i].value;
      if (rem < 2 || rem > 50)
        continue;

      const char *d2_name = get_double_target_name(rem);
      if (d2_name != NULL) {
        char temp[MAX_OUT_STR_LEN];
        snprintf(temp, sizeof(temp), "%s - %s", ALL_TARGETS[i].name, d2_name);
        bool dup = false;
        for (uint8_t m = 0; m < count; m++) {
          if (strcmp(outs[m].str, temp) == 0) {
            dup = true;
            break;
          }
        }
        if (!dup) {
          snprintf(outs[count].str, MAX_OUT_STR_LEN, "%s", temp);
          outs[count].darts_count = 2;
          count++;
        }
      }
    }
  }

  // 3-Dart Outs (Dart 1: Any, Dart 2: Any, Dart 3: Double)
  if (count < MAX_OUT_COMBINATIONS) {
    for (size_t i = 0; i < TARGETS_COUNT && count < MAX_OUT_COMBINATIONS; i++) {
      int32_t rem1 = score - ALL_TARGETS[i].value;
      if (rem1 < 4 || rem1 > 110)
        continue;

      for (size_t j = 0; j < TARGETS_COUNT && count < MAX_OUT_COMBINATIONS;
           j++) {
        int32_t rem2 = rem1 - ALL_TARGETS[j].value;
        if (rem2 < 2 || rem2 > 50)
          continue;

        const char *d3_name = get_double_target_name(rem2);
        if (d3_name != NULL) {
          char temp[MAX_OUT_STR_LEN];
          snprintf(temp, sizeof(temp), "%s - %s - %s", ALL_TARGETS[i].name,
                   ALL_TARGETS[j].name, d3_name);
          bool dup = false;
          for (uint8_t m = 0; m < count; m++) {
            if (strcmp(outs[m].str, temp) == 0) {
              dup = true;
              break;
            }
          }
          if (!dup) {
            snprintf(outs[count].str, MAX_OUT_STR_LEN, "%s", temp);
            outs[count].darts_count = 3;
            count++;
          }
        }
      }
    }
  }

  return count;
}

bool darts_engine_submit_turn(darts_game_state_t *state, int32_t turn_score) {
  if (!state || state->is_leg_finished) {
    return false;
  }

  state->last_turn_score = turn_score;

  int32_t new_score = state->current_score - turn_score;

  // Bust rules:
  // 1. New score < 0 (went over)
  // 2. New score == 1 (cannot finish on 1)
  if (new_score < 0 || new_score == 1) {
    state->is_busted = true;
    return false;
  }

  // Leg finished on exactly 0
  if (new_score == 0) {
    state->current_score = 0;
    state->is_leg_finished = true;
    state->is_busted = false;
    state->outs_count = 0;
    return true;
  }

  // Valid intermediate score
  state->current_score = new_score;
  state->is_busted = false;
  state->outs_count =
      darts_engine_get_checkouts(state->current_score, state->outs);
  return true;
}
