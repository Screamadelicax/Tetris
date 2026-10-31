#pragma once
#include <stdint.h>

namespace tetris {

const int8_t W = 8;    // board columns
const int8_t H = 12;   // board rows (row 0 = top)
const uint8_t FULL_ROW = 0xFF;

struct Cell { int8_t x, y; };   // x = column, y = row (y grows downward)

// The T and I pieces were reordered so the pivot is a middle square, which
// makes them spin in place instead of swinging around an end square.
const Cell SHAPES[7][4] = {
  {{ 0, 1}, { 0, 0}, { 0, 2}, { 0, 3}},   // 0 I
  {{-1, 1}, {-1, 0}, {-1, 2}, { 0, 1}},   // 1 T
  {{ 0, 0}, { 0, 1}, { 1, 0}, { 1, 1}},   // 2 O (square) — never rotates
  {{ 0, 1}, { 0, 0}, { 0, 2}, { 1, 0}},   // 3 left L
  {{ 0, 1}, { 0, 0}, { 0, 2}, {-1, 0}},   // 4 right L
  {{ 0, 1}, { 0, 0}, { 1, 1}, { 1, 2}},   // 5 left Z
  {{ 0, 1}, { 0, 0}, {-1, 1}, {-1, 2}},   // 6 right Z
};
const uint8_t O_PIECE = 2;
const int8_t SPAWN_X = 4;   
const int8_t SPAWN_Y = 0;

// Input actions (bitmask), produced by the buttons / Serial in the sketch.
enum : uint8_t {
  A_LEFT = 1, A_RIGHT = 2, A_ROTATE = 4, A_DOWN = 8, A_DROP = 16,
};

enum State : uint8_t { PLAYING, CLEARING, GAME_OVER };

// Timing (milliseconds)
const uint32_t CLEAR_ANIM_MS = 450;    // flash full rows before removing
const uint32_t GAME_OVER_FLASH_MS = 1500;
const uint32_t RESTART_LOCKOUT_MS = 1000; // ignore buttons right after dying
const uint32_t SCROLL_STEP_MS = 110;      // score scroll speed

// 3x5 digit font for the score shown after game over.
// Each digit = 5 rows of 3 bits, MSB = left column.
const uint8_t DIGITS[10][5] = {
  {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
  {7,4,7,1,7}, {7,4,7,5,7}, {7,1,2,2,2}, {7,5,7,5,7}, {7,5,7,1,7},
};

class Game {
public:
  typedef long (*RandFn)(long n);   // must return a value in [0, n)

  void begin(uint32_t now, RandFn rnd) {
    rnd_ = rnd;
    for (int8_t r = 0; r < H; r++) board_[r] = 0;
    score_ = 0; lines_ = 0;
    bagLeft_ = 0;
    clearMask_ = 0;
    lastFall_ = now;
    if (spawn()) setState(PLAYING, now); else setState(GAME_OVER, now);
  }

  // Call every loop with the actions that fired since the last call.
  void update(uint32_t now, uint8_t actions) {
    switch (state_) {

      case GAME_OVER:
        if (actions && now - since_ >= RESTART_LOCKOUT_MS) begin(now, rnd_);
        return;

      case CLEARING:
        if (now - since_ >= CLEAR_ANIM_MS) {
          collapseRows();
          nextPiece(now);
        }
        return;

      case PLAYING:
        if (actions & A_LEFT)   move(-1, 0);
        if (actions & A_RIGHT)  move(1, 0);
        if (actions & A_ROTATE) rotate();
        if (actions & A_DROP) {                 // SPACE in the Java version
          int8_t d = 0;
          while (move(0, 1)) d++;
          score_ += 2 * d;
          lockPiece(now);
          return;
        }
        if (actions & A_DOWN) {                 // soft drop
          if (move(0, 1)) { score_ += 1; lastFall_ = now; }
        }
        if (now - lastFall_ >= fallInterval()) {   // the Timeline KeyFrame
          lastFall_ = now;
          if (!move(0, 1)) lockPiece(now);
        }
        return;
    }
  }

  void render(uint32_t now, uint8_t rows[H]) const {
    for (int8_t r = 0; r < H; r++) rows[r] = board_[r];
    uint32_t t = now - since_;

    switch (state_) {
      case PLAYING:
        drawPiece(rows);
        break;

      case CLEARING:               // full rows flash on/off
        if ((t / 75) % 2)
          for (int8_t r = 0; r < H; r++)
            if (clearMask_ & (1u << r)) rows[r] = 0;
        break;

      case GAME_OVER:
        if (t < GAME_OVER_FLASH_MS) {
          drawPiece(rows);
          if ((t / 150) % 2) for (int8_t r = 0; r < H; r++) rows[r] = 0;
        } else {
          drawScore(t - GAME_OVER_FLASH_MS, rows);
        }
        break;
    }
  }

  State state() const { return state_; }
  uint32_t score() const { return score_; }
  uint16_t lines() const { return lines_; }
  uint8_t level() const { return lines_ / 4; }

  // Gravity: starts at 800 ms per row, speeds up
  // every 4 lines, never faster than 120 ms.
  uint32_t fallInterval() const {
    int32_t ms = 800 - 70 * (int32_t)level();
    return ms < 120 ? 120 : (uint32_t)ms;
  }

  // ---- exposed for tests ----
  const uint8_t* board() const { return board_; }
  uint8_t* boardMutable() { return board_; }
  const Cell* piece() const { return piece_; }
  uint8_t pieceType() const { return type_; }
  bool fits(const Cell c[4]) const {
    for (uint8_t i = 0; i < 4; i++) {
      if (c[i].x < 0 || c[i].x >= W || c[i].y < 0 || c[i].y >= H) return false;
      if (board_[c[i].y] & (1u << c[i].x)) return false;
    }
    return true;
  }
  bool move(int8_t dx, int8_t dy) {   // leftMovable/rightMovable/downMovable
    Cell n[4];
    for (uint8_t i = 0; i < 4; i++) { n[i].x = piece_[i].x + dx; n[i].y = piece_[i].y + dy; }
    if (!fits(n)) return false;
    for (uint8_t i = 0; i < 4; i++) piece_[i] = n[i];
    return true;
  }
  // Same formula as Pieces.rotate(), in grid units:
  //   newX = cx - cy + oldY,  newY = cy + cx - oldX
  // plus a small "wall kick": if the rotated piece hits a wall/block,
  // try nudging it one column left or right before giving up.
  bool rotate() {
    if (type_ == O_PIECE) return false;
    int8_t cx = piece_[0].x, cy = piece_[0].y;
    Cell n[4];
    for (uint8_t i = 0; i < 4; i++) {
      n[i].x = cx - cy + piece_[i].y;
      n[i].y = cy + cx - piece_[i].x;
    }
    static const int8_t kicks[] = {0, -1, 1, -2, 2};
    for (uint8_t k = 0; k < sizeof(kicks); k++) {
      Cell t[4];
      for (uint8_t i = 0; i < 4; i++) { t[i].x = n[i].x + kicks[k]; t[i].y = n[i].y; }
      // the I piece is the only one that needs a 2-column kick
      if ((kicks[k] == 2 || kicks[k] == -2) && type_ != 0) continue;
      if (fits(t)) { for (uint8_t i = 0; i < 4; i++) piece_[i] = t[i]; return true; }
    }
    return false;
  }

private:
  void setState(State s, uint32_t now) { state_ = s; since_ = now; }

  // 7-bag randomizer: every piece shows up once per 7 spawns, so you never
  // wait forever for an I piece (the Java version used Math.random()*7).
  uint8_t drawFromBag() {
    if (bagLeft_ == 0) {
      for (uint8_t i = 0; i < 7; i++) bag_[i] = i;
      for (int8_t i = 6; i > 0; i--) {
        uint8_t j = (uint8_t)rnd_(i + 1);
        uint8_t tmp = bag_[i]; bag_[i] = bag_[j]; bag_[j] = tmp;
      }
      bagLeft_ = 7;
    }
    return bag_[--bagLeft_];
  }

  // generatePieces(). Returns false if the new piece doesn't fit = game over.
  bool spawn() {
    type_ = drawFromBag();
    for (uint8_t i = 0; i < 4; i++) {
      piece_[i].x = SPAWN_X + SHAPES[type_][i].x;
      piece_[i].y = SPAWN_Y + SHAPES[type_][i].y;
    }
    return fits(piece_);
  }

  void nextPiece(uint32_t now) {
    lastFall_ = now;
    if (spawn()) setState(PLAYING, now); else setState(GAME_OVER, now);
  }

  // stopPiece() + the start of clearLine()
  void lockPiece(uint32_t now) {
    for (uint8_t i = 0; i < 4; i++) board_[piece_[i].y] |= (1u << piece_[i].x);
    clearMask_ = 0;
    uint8_t n = 0;
    for (int8_t r = 0; r < H; r++)
      if (board_[r] == FULL_ROW) { clearMask_ |= (1u << r); n++; }
    if (n) {
      static const uint16_t PTS[5] = {0, 40, 100, 300, 1200};  // classic scoring
      score_ += (uint32_t)PTS[n] * (level() + 1);
      lines_ += n;
      setState(CLEARING, now);
    } else {
      nextPiece(now);
    }
  }

  // Remove full rows and drop everything above them (clearLine()).
  void collapseRows() {
    int8_t dst = H - 1;
    for (int8_t src = H - 1; src >= 0; src--)
      if (!(clearMask_ & (1u << src))) board_[dst--] = board_[src];
    while (dst >= 0) board_[dst--] = 0;
    clearMask_ = 0;
  }

  void drawPiece(uint8_t rows[H]) const {
    for (uint8_t i = 0; i < 4; i++)
      if (piece_[i].y >= 0 && piece_[i].y < H) rows[piece_[i].y] |= (1u << piece_[i].x);
  }

  // Score scrolls right-to-left through the middle of the screen.
  void drawScore(uint32_t t, uint8_t rows[H]) const {
    char buf[11]; uint8_t n = 0;
    uint32_t s = score_;
    do { buf[n++] = '0' + s % 10; s /= 10; } while (s && n < 10);
    // buf is reversed
    int16_t textW = 4 * n;                 // 3 px glyph + 1 px gap
    int16_t span = textW + W;
    int16_t offset = (int16_t)((t / SCROLL_STEP_MS) % span);
    int16_t startX = W - offset;
    const int8_t top = 3;                  // rows 3..7, vertically centred
    for (int8_t r = 0; r < H; r++) rows[r] = 0;
    for (int8_t c = 0; c < W; c++) {
      int16_t tx = c - startX;
      if (tx < 0 || tx >= textW || tx % 4 == 3) continue;
      uint8_t d = buf[n - 1 - tx / 4] - '0';
      uint8_t gc = tx % 4;
      for (int8_t gr = 0; gr < 5; gr++)
        if ((DIGITS[d][gr] >> (2 - gc)) & 1) rows[top + gr] |= (1u << c);
    }
  }

  RandFn rnd_ = 0;
  uint8_t board_[H] = {0};
  Cell piece_[4];
  uint8_t type_ = 0;
  uint8_t bag_[7];
  uint8_t bagLeft_ = 0;
  uint16_t clearMask_ = 0;
  State state_ = PLAYING;
  uint32_t since_ = 0, lastFall_ = 0;
  uint32_t score_ = 0;
  uint16_t lines_ = 0;
};

}  // namespace tetris
