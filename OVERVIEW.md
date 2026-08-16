# Darts

This is an ESP32-S3 based darts practice scoreboard. It's built on the Guition JC8048W550 development board.

This board is described in https://github.com/rzeldent/platformio-espressif32-sunton, this is a library for Arduino/PlatformIO development, but this project is ESP-IDF 6.0.2 based instead. Arduino overhead causes a lot of screen tearing on this hardware.

The ESP-IDF plugin is available in the IDE, but the project files have not been created yet. The SDK is installed at `C:\esp\v6.0.2\esp-idf`.

The repo has been cloned into `.platformio-espressif32-sunton/` and the board's description file is `JC8048W550C.json`.

The scope of this project is simple for now, implement a score display that shows the player's current score (starting at 301 and counting down) on the left side of the screen, and a keypad for entering your last move on the right side.

The display is about 7" diagonal and will be in a horizontal orientation. Make the numbers large enough to be visible from a few meters away. The keypad should have 0-9 in a numpad style layout with clear and submit buttons at the bottom, flanking the zero key. Make this keypad work for numeric entry with the enter key submitting a move. Use Segoe UI for the numbers on the display and for the number buttons. (Clear and submit can be smaller). Nothing should be "cute" here, this is functional. Using a light background color (rgb(240, 240, 240)) for the main background for now, but keep open the option to do dark mode later if I change my mind. The keypad should be a custom control, not rely on the LVGL built-in keypad. The keypad layout is as follows:

```
1 2 3
4 5 6
7 8 9
  0
```

Clear and Submit will be below the 0 key, one on each side.

When the player's current score is within range to finish in the current turn, show a list of the "outs" (the combinations that will finish the game) below the score. Show the options as separate items, with the most probable option (the one requiring the fewest darts) listed first, and separated by horizontal rules. In this ruleset, an "out" must contain at least one double or triple position to finish the game. Going over the remaining score is a bust, and the score should remain unchanged for that turn. You cannot finish on a single one of anything, you must use a double or triple to finish.