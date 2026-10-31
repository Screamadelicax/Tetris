# Tetris on the Arduino UNO R4 WiFi LED matrix

## Files
| File | What it is |
|---|---|
| `tetris_r4.ino` | Hardware layer: buttons, Serial Monitor controls, LED matrix drawing |
| `TetrisGame.h` | Game logic (board, pieces, rotation, line clears, scoring, game states). No Arduino code, so it can be tested on a laptop |

## Setup
1. Arduino IDE → Boards Manager → install **Arduino UNO R4 Boards** → select *Arduino UNO R4 WiFi*.
   (`Arduino_LED_Matrix` comes with that board package; no extra libraries needed.)
2. Open `tetris_r4/tetris_r4.ino` (the folder name must match the .ino name) and upload.
3. Hold the board **upright, USB-C port pointing down**: the matrix is then 8 wide × 12 tall.
   If pieces fall the wrong way, set `#define FLIP_180 1` and re-upload.

| Pin | Button | Held = auto-repeat? |
|---|---|---|
| D2 | Left | yes |
| D3 | Right | yes |
| D4 | Rotate | no |
| D5 | Soft drop (down) | yes |
| D6 | Hard drop (the old SPACE key) | no |

**No buttons yet?** Open the Serial Monitor at 115200 baud and type
`a` left, `d` right, `w` rotate, `s` down, `x` hard drop, then press Enter.

## Versus mode

After reset, use Left/Right to choose single-player or versus mode, then press
Rotate to confirm. In versus mode, both UNO R4 WiFi boards connect to the same phone hotspot.
Set `WIFI_SSID` and `WIFI_PASSWORD` in `VersusGame.h` to your hotspot
credentials. The boards discover each other with UDP broadcast on port 4210;
neither board creates a WiFi access point. Keep client/AP isolation disabled
on the phone hotspot. Clearing lines sends one garbage row per cleared line to
the opponent. A versus game ends when one board reaches the top; reset either
board to return to the mode menu.

## Game states (FSM)
`PLAYING` → (piece locks with full rows) → `CLEARING` (rows flash 450 ms) → `PLAYING`
`PLAYING`/`CLEARING` → (new piece doesn't fit) → `GAME_OVER` → (any button after 1 s) → `PLAYING`

## Circuit
https://app.cirkitdesigner.com/project/9eb23c06-6c46-4f0a-8e20-9ddbd20e1ab2