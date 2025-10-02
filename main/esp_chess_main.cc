/*
 * ESP32 Chess Game
 * Main entry point for ESP-IDF
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <iostream>
#include <sstream>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "Chess.h"
#include "ChessBoard.hh"
#include "ChessPiece.hh"

using namespace Student;
using namespace std;

static const char *TAG = "ESP_CHESS";

// UART configuration
#define UART_NUM UART_NUM_0
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)

// Forward declarations
void setupStandardBoard(ChessBoard& board);
bool isKingInCheck(ChessBoard& board, Color color);
bool hasValidMoves(ChessBoard& board, Color color);
string colorToString(Color color);
void makeAIMove(ChessBoard& board);

// Setup standard chess board
void setupStandardBoard(ChessBoard& board) {
    // Setup white pieces (bottom rows 6-7)
    // Pawns
    for (int col = 0; col < 8; col++) {
        board.createChessPiece(White, Pawn, 6, col);
    }

    // Back row
    board.createChessPiece(White, Rook, 7, 0);
    board.createChessPiece(White, Knight, 7, 1);
    board.createChessPiece(White, Bishop, 7, 2);
    board.createChessPiece(White, Queen, 7, 3);
    board.createChessPiece(White, King, 7, 4);
    board.createChessPiece(White, Bishop, 7, 5);
    board.createChessPiece(White, Knight, 7, 6);
    board.createChessPiece(White, Rook, 7, 7);

    // Setup black pieces (top rows 0-1)
    // Pawns
    for (int col = 0; col < 8; col++) {
        board.createChessPiece(Black, Pawn, 1, col);
    }

    // Back row
    board.createChessPiece(Black, Rook, 0, 0);
    board.createChessPiece(Black, Knight, 0, 1);
    board.createChessPiece(Black, Bishop, 0, 2);
    board.createChessPiece(Black, Queen, 0, 3);
    board.createChessPiece(Black, King, 0, 4);
    board.createChessPiece(Black, Bishop, 0, 5);
    board.createChessPiece(Black, Knight, 0, 6);
    board.createChessPiece(Black, Rook, 0, 7);
}

// Check if king is in check
bool isKingInCheck(ChessBoard& board, Color color) {
    // Find the king
    int kingRow = -1, kingCol = -1;
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            ChessPiece* piece = board.getPiece(row, col);
            if (piece && piece->getColor() == color && piece->getType() == King) {
                kingRow = row;
                kingCol = col;
                break;
            }
        }
        if (kingRow != -1) break;
    }

    if (kingRow == -1) return false; // No king found

    // Check if any opponent piece can capture the king
    Color opponentColor = (color == White) ? Black : White;
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            ChessPiece* piece = board.getPiece(row, col);
            if (piece && piece->getColor() == opponentColor) {
                if (piece->canMoveToLocation(kingRow, kingCol)) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Check if player has any valid moves
bool hasValidMoves(ChessBoard& board, Color color) {
    for (int fromRow = 0; fromRow < 8; fromRow++) {
        for (int fromCol = 0; fromCol < 8; fromCol++) {
            ChessPiece* piece = board.getPiece(fromRow, fromCol);
            if (piece && piece->getColor() == color) {
                // Try all possible destinations
                for (int toRow = 0; toRow < 8; toRow++) {
                    for (int toCol = 0; toCol < 8; toCol++) {
                        if (piece->canMoveToLocation(toRow, toCol)) {
                            // Temporarily make the move
                            ChessPiece* capturedPiece = board.getPiece(toRow, toCol);
                            board.movePiece(fromRow, fromCol, toRow, toCol);

                            // Check if king is still in check
                            bool stillInCheck = isKingInCheck(board, color);

                            // Undo the move
                            board.movePiece(toRow, toCol, fromRow, fromCol);
                            if (capturedPiece) {
                                board.createChessPiece(capturedPiece->getColor(),
                                                     capturedPiece->getType(),
                                                     toRow, toCol);
                            }

                            if (!stillInCheck) {
                                return true; // Found a valid move
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

// Convert color to string
string colorToString(Color color) {
    return (color == White) ? "White" : "Black";
}

// Simple AI move (random valid move)
void makeAIMove(ChessBoard& board) {
    for (int fromRow = 0; fromRow < 8; fromRow++) {
        for (int fromCol = 0; fromCol < 8; fromCol++) {
            ChessPiece* piece = board.getPiece(fromRow, fromCol);
            if (piece && piece->getColor() == Black) {
                for (int toRow = 0; toRow < 8; toRow++) {
                    for (int toCol = 0; toCol < 8; toCol++) {
                        if (board.movePiece(fromRow, fromCol, toRow, toCol)) {
                            printf("AI moves from (%d,%d) to (%d,%d)\n",
                                   fromRow, fromCol, toRow, toCol);
                            return;
                        }
                    }
                }
            }
        }
    }
    printf("AI has no valid moves!\n");
}

// Read line from UART with echo
void uart_read_line(char* buffer, int max_len) {
    int pos = 0;
    while (pos < max_len - 1) {
        uint8_t c;
        int len = uart_read_bytes(UART_NUM, &c, 1, portMAX_DELAY);
        if (len > 0) {
            // Echo the character back
            uart_write_bytes(UART_NUM, (const char*)&c, 1);

            if (c == '\n' || c == '\r') {
                uart_write_bytes(UART_NUM, "\r\n", 2);
                break;
            } else if (c == '\b' || c == 127) { // Backspace
                if (pos > 0) {
                    pos--;
                    uart_write_bytes(UART_NUM, "\b \b", 3);
                }
            } else if (c >= 32 && c < 127) { // Printable character
                buffer[pos++] = c;
            }
        }
    }
    buffer[pos] = '\0';
}

// Print to UART
void uart_printf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    uart_write_bytes(UART_NUM, buffer, strlen(buffer));
}

// Chess game task
void chess_game_task(void *pvParameter) {
    ChessBoard board(8, 8);
    setupStandardBoard(board);

    Color currentPlayer = White;
    bool gameRunning = true;
    char input[256];

    uart_printf("\r\n======================================\r\n");
    uart_printf("       WELCOME TO ESP32 CHESS        \r\n");
    uart_printf("======================================\r\n");
    uart_printf("Enter moves as: row1 col1 row2 col2\r\n");
    uart_printf("Example: 6 4 4 4 (moves pawn from 6,4 to 4,4)\r\n");
    uart_printf("Commands: quit, help, board, new, ai\r\n");
    uart_printf("======================================\r\n\r\n");

    // Display initial board
    string boardStr = board.displayBoard().str();
    uart_printf("%s\r\n", boardStr.c_str());

    while (gameRunning) {
        // Check game state
        bool inCheck = isKingInCheck(board, currentPlayer);
        bool hasValidMove = hasValidMoves(board, currentPlayer);

        if (inCheck && !hasValidMove) {
            uart_printf("\r\nCHECKMATE! %s wins!\r\n",
                       colorToString(currentPlayer == White ? Black : White).c_str());
            uart_printf("Type 'new' for a new game or 'quit' to exit: ");
            uart_read_line(input, sizeof(input));

            if (strcmp(input, "new") == 0) {
                board = ChessBoard(8, 8);
                setupStandardBoard(board);
                currentPlayer = White;
                uart_printf("\r\n=== NEW GAME ===\r\n");
                boardStr = board.displayBoard().str();
                uart_printf("%s\r\n", boardStr.c_str());
                continue;
            } else {
                break;
            }
        } else if (!hasValidMove) {
            uart_printf("\r\nSTALEMATE! It's a draw!\r\n");
            uart_printf("Type 'new' for a new game or 'quit' to exit: ");
            uart_read_line(input, sizeof(input));

            if (strcmp(input, "new") == 0) {
                board = ChessBoard(8, 8);
                setupStandardBoard(board);
                currentPlayer = White;
                uart_printf("\r\n=== NEW GAME ===\r\n");
                boardStr = board.displayBoard().str();
                uart_printf("%s\r\n", boardStr.c_str());
                continue;
            } else {
                break;
            }
        } else if (inCheck) {
            uart_printf("\r\n*** CHECK! ***\r\n");
        }

        uart_printf("\r\n%s's turn.\r\n", colorToString(currentPlayer).c_str());
        uart_printf("Enter move: ");
        uart_read_line(input, sizeof(input));

        if (strcmp(input, "quit") == 0) {
            uart_printf("Thanks for playing!\r\n");
            break;
        } else if (strcmp(input, "help") == 0) {
            uart_printf("\r\n=== CHESS COMMANDS ===\r\n");
            uart_printf("Move format: row1 col1 row2 col2\r\n");
            uart_printf("  Example: 6 4 4 4\r\n");
            uart_printf("Special commands:\r\n");
            uart_printf("  'quit'  - Exit the game\r\n");
            uart_printf("  'help'  - Show this help\r\n");
            uart_printf("  'board' - Redraw the board\r\n");
            uart_printf("  'new'   - Start a new game\r\n");
            uart_printf("  'ai'    - Let AI make a move (Black)\r\n");
            uart_printf("======================\r\n");
            continue;
        } else if (strcmp(input, "board") == 0) {
            boardStr = board.displayBoard().str();
            uart_printf("\r\n%s\r\n", boardStr.c_str());
            continue;
        } else if (strcmp(input, "new") == 0) {
            board = ChessBoard(8, 8);
            setupStandardBoard(board);
            currentPlayer = White;
            uart_printf("\r\n=== NEW GAME ===\r\n");
            boardStr = board.displayBoard().str();
            uart_printf("%s\r\n", boardStr.c_str());
            continue;
        } else if (strcmp(input, "ai") == 0 && currentPlayer == Black) {
            makeAIMove(board);
            currentPlayer = (currentPlayer == White) ? Black : White;
            boardStr = board.displayBoard().str();
            uart_printf("%s\r\n", boardStr.c_str());
            continue;
        }

        // Parse move input
        int fromRow, fromCol, toRow, toCol;
        if (sscanf(input, "%d %d %d %d", &fromRow, &fromCol, &toRow, &toCol) != 4) {
            uart_printf("Invalid input format. Use: row1 col1 row2 col2\r\n");
            continue;
        }

        // Validate piece belongs to current player
        ChessPiece* piece = board.getPiece(fromRow, fromCol);
        if (!piece) {
            uart_printf("No piece at position (%d,%d)\r\n", fromRow, fromCol);
            continue;
        }

        if (piece->getColor() != currentPlayer) {
            uart_printf("That's not your piece!\r\n");
            continue;
        }

        // Try to make the move
        if (board.movePiece(fromRow, fromCol, toRow, toCol)) {
            // Check if move puts own king in check
            if (isKingInCheck(board, currentPlayer)) {
                // Undo the move
                board.movePiece(toRow, toCol, fromRow, fromCol);
                uart_printf("Invalid move: would put your king in check!\r\n");
                continue;
            }

            // Move successful, switch players
            currentPlayer = (currentPlayer == White) ? Black : White;
            boardStr = board.displayBoard().str();
            uart_printf("\r\n%s\r\n", boardStr.c_str());
        } else {
            uart_printf("Invalid move!\r\n");
        }

        // Small delay to prevent watchdog issues
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    uart_printf("\r\nGame ended. ESP32 will restart in 5 seconds...\r\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    esp_restart();
}

extern "C" {
    void app_main(void);
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting ESP32 Chess Game");

    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Create chess game task
    xTaskCreate(chess_game_task, "chess_game", 8192, NULL, 10, NULL);
}