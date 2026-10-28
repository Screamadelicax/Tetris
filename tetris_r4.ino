// Hold the board upright: rotate it 90 degrees counter-clockwise from the
// usual "USB on the left" position, so the USB-C port points DOWN. The
// matrix is then 8 columns wide and 12 rows tall. If the game looks
// upside-down, set FLIP_180 to 1.
//
// Controls: push-buttons from each pin to GND (internal pull-ups, no
// resistors needed). Alternatively type into the Serial Monitor at
// 115200 baud: a = left, d = right, w = rotate, s = down,
// x = hard drop
//
// The game logic lives in TetrisGame.h
// This file only handles the hardware: buttons, Serial and the LED matrix.

#include "Arduino_LED_Matrix.h"
#include "TetrisGame.h"

#define FLIP_180 0

// ---------- pins ----------
const uint8_t PIN_LEFT   = 2;
const uint8_t PIN_RIGHT  = 3;
const uint8_t PIN_ROTATE = 4;
const uint8_t PIN_DOWN   = 5;
const uint8_t PIN_DROP   = 6;
// ---------- button handling ----------
const uint32_t DEBOUNCE_MS   = 15;
const uint32_t REPEAT_DELAY  = 170;  // hold this long before auto-repeat...
const uint32_t REPEAT_EVERY  = 60;   // ...then repeat this often

struct Button {
  uint8_t pin;
  uint8_t action;
  bool repeats;        // left/right/down auto-repeat when held
  bool stable;         // debounced state (true = pressed)
  bool lastRaw;
  uint32_t lastChange;
  uint32_t pressedAt;
  uint32_t lastRepeat;
};

Button buttons[] = {
  {PIN_LEFT,   tetris::A_LEFT,   true },
  {PIN_RIGHT,  tetris::A_RIGHT,  true },
  {PIN_ROTATE, tetris::A_ROTATE, false},
  {PIN_DOWN,   tetris::A_DOWN,   true },
  {PIN_DROP,   tetris::A_DROP,   false},
};
const uint8_t NUM_BUTTONS = sizeof(buttons) / sizeof(buttons[0]);

uint8_t pollButtons(uint32_t now) {
  uint8_t actions = 0;
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    Button &b = buttons[i];
    bool raw = digitalRead(b.pin) == LOW;          // pressed pulls to GND
    if (raw != b.lastRaw) { b.lastRaw = raw; b.lastChange = now; }
    if (now - b.lastChange >= DEBOUNCE_MS && raw != b.stable) {
      b.stable = raw;
      if (b.stable) {                               // new press
        actions |= b.action;
        b.pressedAt = b.lastRepeat = now;
      }
    } else if (b.stable && b.repeats &&
               now - b.pressedAt >= REPEAT_DELAY &&
               now - b.lastRepeat >= REPEAT_EVERY) {
      actions |= b.action;                          // held: auto-repeat
      b.lastRepeat = now;
    }
  }
  return actions;
}

uint8_t pollSerial() {
  uint8_t actions = 0;
  while (Serial.available()) {
    switch (Serial.read()) {
      case 'a': case 'A': actions |= tetris::A_LEFT;   break;
      case 'd': case 'D': actions |= tetris::A_RIGHT;  break;
      case 'w': case 'W': actions |= tetris::A_ROTATE; break;
      case 's': case 'S': actions |= tetris::A_DOWN;   break;
      case 'x': case 'X': case ' ': actions |= tetris::A_DROP; break;
    }
  }
  return actions;
}

// ---------- display ----------
ArduinoLEDMatrix matrix;
uint8_t frame[8][12];                 // the matrix's native layout: 8 rows x 12 cols
uint8_t shown[tetris::H];             // last thing we drew, to skip redundant redraws
bool firstDraw = true;

void drawRows(const uint8_t rows[tetris::H]) {
  // board (row r, col c), r = 0 at the top, maps to matrix (c, 11 - r):
  // the top of the game is the matrix's right edge.
  for (int8_t r = 0; r < tetris::H; r++) {
    for (int8_t c = 0; c < tetris::W; c++) {
      bool on = (rows[r] >> c) & 1;
#if FLIP_180
      frame[tetris::W - 1 - c][r] = on;
#else
      frame[c][tetris::H - 1 - r] = on;
#endif
    }
  }
  matrix.renderBitmap(frame, 8, 12);
}

// ---------- game ----------
tetris::Game game;
long gameRandom(long n) { return random(n); }

tetris::State lastState;

void setup() {
  Serial.begin(115200);
  matrix.begin();
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) pinMode(buttons[i].pin, INPUT_PULLUP);

  // Floating analog pin + timing = a different piece order each power-up.
  randomSeed(analogRead(A0) ^ micros());

  game.begin(millis(), gameRandom);
  lastState = game.state();
  Serial.println(F("Tetris! a/d move, w rotate, s down, x drop"));
}

void loop() {
  uint32_t now = millis();
  uint8_t actions = pollButtons(now) | pollSerial();
  game.update(now, actions);

  uint8_t rows[tetris::H];
  game.render(now, rows);
  bool changed = firstDraw;
  for (int8_t r = 0; r < tetris::H; r++) if (rows[r] != shown[r]) { changed = true; shown[r] = rows[r]; }
  if (changed) { drawRows(rows); firstDraw = false; }

  // Report to the Serial Monitor
  if (game.state() != lastState) {
    lastState = game.state();
    if (lastState == tetris::GAME_OVER) {
      Serial.print(F("Game over! Score: ")); Serial.print(game.score());
      Serial.print(F("  Lines: "));         Serial.println(game.lines());
      Serial.println(F("Press any button to play again."));
    } else if (lastState == tetris::CLEARING) {
      Serial.print(F("Lines: ")); Serial.print(game.lines());
      Serial.print(F("  Score: ")); Serial.println(game.score());
    }
  }
}
