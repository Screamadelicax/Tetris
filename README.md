# Tetris on the Arduino UNO R4 WiFi LED matrix

A port of the JavaFX Tetris (CS15) to the UNO R4 WiFi's built-in 12×8 LED matrix.
Note: the R4 **Minima** has no LED matrix. This needs the **R4 WiFi**.

## Files
| File | What it is |
|---|---|
| `tetris_r4.ino` | Hardware layer: buttons, Serial Monitor controls, LED matrix drawing |
| `TetrisGame.h` | Game logic (board, pieces, rotation, line clears, scoring, game states). No Arduino code, so it can be tested on a laptop |
| `test/host_test.cpp` | Laptop tests: `g++ -std=c++11 -I.. host_test.cpp && ./a.out` |

## Setup
1. Arduino IDE → Boards Manager → install **Arduino UNO R4 Boards** → select *Arduino UNO R4 WiFi*.
   (`Arduino_LED_Matrix` comes with that board package; no extra libraries needed.)
2. Open `tetris_r4/tetris_r4.ino` (the folder name must match the .ino name) and upload.
3. Hold the board **upright, USB-C port pointing down**: the matrix is then 8 wide × 12 tall.
   If pieces fall the wrong way, set `#define FLIP_180 1` and re-upload.

## Wiring (each button: one leg to the pin, other leg to GND)
Internal pull-ups are used, so no resistors are needed.

| Pin | Button | Held = auto-repeat? |
|---|---|---|
| D2 | Left | yes |
| D3 | Right | yes |
| D4 | Rotate | no |
| D5 | Soft drop (down) | yes |
| D6 | Hard drop (the old SPACE key) | no |
| D7 | Pause (optional) | no |

**No buttons yet?** Open the Serial Monitor at 115200 baud and type
`a` left, `d` right, `w` rotate, `s` down, `x` hard drop, `p` pause, then press Enter.

## How it maps from the Java version
| Java | Arduino |
|---|---|
| `Square[][]` board + "is this square LIGHTPINK?" checks | `uint8_t board[12]`, one byte per row (8 columns = 8 bits); full row = `0xFF` |
| `Constants.*_PIECE_COORDS` | `SHAPES[7][4]` (same shapes, in grid units) |
| `Pieces.leftMovable/rightMovable/downMovable` | `Game::move(dx, dy)` using `fits()` |
| `Pieces.rotate()` formula about `piece[0]` | same formula, plus a one-column wall kick |
| `Timeline` + `KeyFrame` | non-blocking `millis()` timing in `Game::update()` |
| `handleKeyPress` | button debouncing + Serial in the sketch → action bitmask |
| `pPress % 2` pause toggle | `PAUSED` state |
| "Game Over" text | board flashes, then the score scrolls across the screen; any button restarts |

## Game states (FSM)
`PLAYING` → (piece locks with full rows) → `CLEARING` (rows flash 450 ms) → `PLAYING`
`PLAYING` ⇄ `PAUSED` (pause button)
`PLAYING`/`CLEARING` → (new piece doesn't fit) → `GAME_OVER` → (any button after 1 s) → `PLAYING`

## Changes from the Java version
- **Game over** happens when a new piece can't spawn. Before, it was decided by checking row 1.
- **7-bag randomizer**: every piece appears once in every 7 spawns.
- **Speed** starts at 800 ms per row and gets 70 ms faster every 4 lines (minimum 120 ms).
- **Scoring**: 40 / 100 / 300 / 1200 × (level+1) for 1–4 lines, +1 per soft-drop row, +2 per hard-drop row.
- The **T and I pieces** were reordered so they rotate around a middle square.
- The O piece still never rotates.
