# Darts Scoreboard

An ESP-IDF based 301/501 Darts Practice Scoreboard designed for the Guition JC8048W550 7-inch capacitive touch development board powered by the ESP32-S3.

## Overview

This project provides a functional, highly visible 501/301 Darts practice scoreboard optimized for 7-inch 800x480 RGB LCD displays. Unlike Arduino-based implementations, this project is built directly on ESP-IDF v6.0.2 with LVGL v9.2 to eliminate screen tearing, achieve high-performance DMA rendering, and ensure low memory latency.

### AI Disclosure

This project's code, structure, custom font rendering, and hardware integration were fully generated and developed by AI (Google DeepMind's Antigravity AI coding assistant) working in pair-programming mode with the user.

### Credits & Hardware Reference

Hardware specifications and board pinout configurations for the Guition JC8048W550 board are credited to [rzeldent/platformio-espressif32-sunton](https://github.com/rzeldent/platformio-espressif32-sunton).

## Hardware Platform

- **Development Board**: Guition JC8048W550 (ESP32-S3-WROOM-1 N16R8)
- **Processor**: ESP32-S3 Dual-Core Xtensa LX7 @ 240 MHz
- **Display**: 7.0" ST7701S 800x480 RGB 16-bit Parallel LCD
- **Touch Panel**: GT911 Capacitive Touch Controller (I2C)
- **Memory**: 16 MB QIO Flash, 8 MB Octal PSRAM

## Features

- **Large Score Display**: Ultra-large Segoe UI font score display readable from across a room.
- **Darts Game Engine**: Standard 301/501 rules, bust handling, turn history tracking, and leg reset.
- **Checkout Guide ("Outs")**: Displays optimal multi-dart checkout combinations when the current score is <= 170.
- **Custom Touch Numpad**: Responsive 3x4 keypad with Clear and Submit buttons.
- **Settings Overlay**: Accessible by long-pressing the main score display. Provides "Undo Last Turn" and "Start Over" controls.
- **Wi-Fi & OTA Server**: Built-in HTTP server for wireless Over-The-Air (OTA) firmware updates via a web browser.
- **mDNS Support**: Accessible on local network via `http://darts-scoreboard.local/`.

## Building and Flashing

### Prerequisites

- ESP-IDF v6.0.2 configured in your environment.
- Python 3.10+

### Build Firmware

```bash
idf.py build
```

### Flash via Serial

```bash
idf.py -p <PORT> flash monitor
```

### Wireless OTA Flashing

Once connected to Wi-Fi, upload new firmware binaries (`build/DartsScoreboard.bin`) directly via:

```bash
python tools/flash_ota.py
```

or by navigating to `http://darts-scoreboard.local/` in any web browser.

## License

This project is released under the Zero-Clause BSD License (0BSD). You are free to use, copy, modify, and distribute this software for any purpose, with or without fee, without any requirement for attribution. See the [LICENSE](LICENSE) file for details.
