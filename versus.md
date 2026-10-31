This document describes how versus mode should be implemented in the game.

1. At the very beginning of the game, players should be presented with a menu to select whether to play the game in single-player mode or versus mode. The player can select by clicking the move-left or move-right buttons, and confirm by clicking the rotate button.
2. If the player selects versus mode, the game should use Arduino Wifi R4's WiFi capabilities to search for and connect to other players in the vicinity.
3. Once connected, the game officially starts in versus mode, and both players can begin playing against each other.
4. During the game in versus mode, both players play like in single-player mode, but one they do a line clear, the opponent would receive an additional line of blocks with one empty space in it at the bottom of their playfield.
5. The game continues until one player's playfield is filled to the top, at which point the other player is declared the winner, and the loser is declared the loser.
6. Then the game completes, nothing will happen until the player resets the game by clicking the reset button.

The basic game logic between the versus mode and single-player mode are largely the same, so the implementation of versus mode can reuse much of the single-player mode's code, with the addition of network communication and the mechanic of sending extra lines to the opponent upon line clears. The game logic can be written in a seperate file different from `TetrisGame.h` like `VersusGame.h`. 