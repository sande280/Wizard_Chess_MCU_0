# ESP32 Chess Game

This project implements a playable chess game for the ESP32 microcontroller using ESP-IDF.

## Features
- Full chess rules implementation
- Play via serial terminal (115200 baud)
- White vs Black two-player mode
- Simple AI opponent for Black pieces
- Move validation and check/checkmate detection
- Board display with ASCII art

## Building the Project

### Using VS Code ESP-IDF Extension:
1. Open this project folder in VS Code
2. Click the **ESP-IDF: Build** button in the bottom toolbar
3. Wait for compilation to complete

### Using Command Line:
```bash
# Set up ESP-IDF environment
C:\Users\jacks\esp\v5.5.1\esp-idf\export.bat

# Build the project
idf.py build
```

## Flashing to ESP32

1. Connect your ESP32 to your computer via USB
2. Find your COM port (e.g., COM3, COM4)
3. In VS Code: Click **ESP-IDF: Flash** button and select your COM port
4. Or via command line:
   ```bash
   idf.py -p COM3 flash
   ```

## Playing the Game

1. After flashing, open a serial monitor:
   - In VS Code: Click **ESP-IDF: Monitor**
   - Or via command line: `idf.py -p COM3 monitor`
   - Or use any serial terminal at 115200 baud

2. The chess board will be displayed with coordinates:
   ```
   0-7 (rows) x 0-7 (columns)
   Row 0-1: Black pieces
   Row 6-7: White pieces
   ```

3. Enter moves in format: `row1 col1 row2 col2`
   - Example: `6 4 4 4` moves white pawn from (6,4) to (4,4)

4. Available commands:
   - `help` - Show command list
   - `board` - Redraw the current board
   - `new` - Start a new game
   - `ai` - Let AI make a move (when playing as Black)
   - `quit` - Exit game (ESP32 will restart)

## Board Layout

```
  0 1 2 3 4 5 6 7
0 R N B Q K B N R  (Black pieces)
1 P P P P P P P P
2 . . . . . . . .
3 . . . . . . . .
4 . . . . . . . .
5 . . . . . . . .
6 P P P P P P P P  (White pieces)
7 R N B Q K B N R

Legend:
K = King   Q = Queen
R = Rook   B = Bishop
N = Knight P = Pawn
```

## Game Rules

- White always moves first
- Standard chess rules apply
- The game detects:
  - Check (king under attack)
  - Checkmate (no legal moves to escape check)
  - Stalemate (no legal moves but not in check)
- Moves that would put your own king in check are not allowed

## Troubleshooting

1. **Build errors**: Make sure all `.cc` and `.hh` files are in the `main/` folder
2. **Serial communication issues**: Ensure baud rate is set to 115200
3. **Game not responding**: Check that you're entering moves in the correct format
4. **ESP32 keeps restarting**: This may indicate a memory issue - the game uses about 8KB of stack

## Files Structure

- `esp_chess_main.cc` - Main ESP-IDF entry point and game loop
- `ChessBoard.cc/hh` - Chess board logic and display
- `ChessPiece.cc/hh` - Base chess piece class
- `*Piece.cc/hh` - Individual piece implementations (King, Queen, Rook, etc.)
- `Chess.h` - Type definitions and enums

## Memory Usage

The chess game task is allocated 8192 bytes of stack space. The game state and board representation use heap memory dynamically allocated during runtime.