#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <utility>
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
#include "BoardStateTracker.hh"
#include "BoardStates.hh"
#include "PathAnalyzer.hh"
#include "ChessAI.hh"

using namespace Student;
using namespace std;

static const char *TAG = "ESP_CHESS";

#define UART_NUM UART_NUM_0
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)


//Bit sequences for pieces
//000 or 111 = emtpy space
//001 = Pawn
//010 = Bishop
//011 = Knight
//100 = Rook
//101 = Queen
//110 = King

//Bit sequences for color
//0000 = Off
//0001 = Red
//1111 = White




void setupStandardBoard(ChessBoard& board);
bool isKingInCheck(ChessBoard& board, Color color);
bool hasValidMoves(ChessBoard& board, Color color);
string colorToString(Color color);
int translateColumn(int displayCol);
void uart_printf(const char* format, ...);
void detectAndExecuteMove(ChessBoard& board, int row1, int col1, int row2, int col2, Color& currentPlayer);
void makeAIMove(ChessBoard& board, Color& currentPlayer);
void makeMinimaxMove(ChessBoard& board, Color& currentPlayer, int depth);

void setupStandardBoard(ChessBoard& board) {
    for (int col = 0; col < 8; col++) {
        board.createChessPiece(White, Pawn, 6, col);
    }

    board.createChessPiece(White, Rook, 7, 0);
    board.createChessPiece(White, Knight, 7, 1);
    board.createChessPiece(White, Bishop, 7, 2);
    board.createChessPiece(White, Queen, 7, 3);
    board.createChessPiece(White, King, 7, 4);
    board.createChessPiece(White, Bishop, 7, 5);
    board.createChessPiece(White, Knight, 7, 6);
    board.createChessPiece(White, Rook, 7, 7);

    for (int col = 0; col < 8; col++) {
        board.createChessPiece(Black, Pawn, 1, col);
    }

    board.createChessPiece(Black, Rook, 0, 0);
    board.createChessPiece(Black, Knight, 0, 1);
    board.createChessPiece(Black, Bishop, 0, 2);
    board.createChessPiece(Black, Queen, 0, 3);
    board.createChessPiece(Black, King, 0, 4);
    board.createChessPiece(Black, Bishop, 0, 5);
    board.createChessPiece(Black, Knight, 0, 6);
    board.createChessPiece(Black, Rook, 0, 7);
}

bool isKingInCheck(ChessBoard& board, Color color) {
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

    if (kingRow == -1) return false;

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

bool hasValidMoves(ChessBoard& board, Color color) {
    Color savedTurn = board.getTurn();
    board.setTurn(color);

    for (int fromRow = 0; fromRow < 8; fromRow++) {
        for (int fromCol = 0; fromCol < 8; fromCol++) {
            ChessPiece* piece = board.getPiece(fromRow, fromCol);
            if (piece && piece->getColor() == color) {
                std::vector<std::pair<int, int>> possibleMoves = board.getPossibleMoves(fromRow, fromCol);
                if (!possibleMoves.empty()) {
                    board.setTurn(savedTurn);
                    return true;
                }
            }
        }
    }

    board.setTurn(savedTurn);
    return false;
}

string colorToString(Color color) {
    return (color == White) ? "White" : "Black";
}

int translateColumn(int displayCol) {
    if (displayCol < 2 || displayCol > 9) {
        return -1;
    }
    return displayCol - 2;
}

// Simple AI structure and function
namespace Student {
    struct SimpleMove {
        int fromRow, fromCol, toRow, toCol;
        int value;
    };
    SimpleMove findBestSimpleMove(ChessBoard& board, Color aiColor);
}

void makeAIMove(ChessBoard& board, Color& currentPlayer) {
    uart_printf("\r\nAI is thinking...\r\n");

    SimpleMove bestMove = findBestSimpleMove(board, currentPlayer);

    if (bestMove.fromRow == -1) {
        uart_printf("AI has no valid moves!\r\n");
        return;
    }

    ChessPiece* aiPiece = board.getPiece(bestMove.fromRow, bestMove.fromCol);
    ChessPiece* targetPiece = board.getPiece(bestMove.toRow, bestMove.toCol);
    bool isCapture = (targetPiece != nullptr);

    uart_printf("AI moves from (%d,%d) to (%d,%d)\r\n",
               bestMove.fromRow, bestMove.fromCol + 2,
               bestMove.toRow, bestMove.toCol + 2);

    board.setTurn(currentPlayer);
    if (board.movePiece(bestMove.fromRow, bestMove.fromCol, bestMove.toRow, bestMove.toCol)) {
        if (isCapture && aiPiece && targetPiece) {
            auto captureDestination = PathAnalyzer::findCaptureZoneDestination(board, targetPiece->getColor());
            PathType capturePath = PathAnalyzer::analyzeMovePath(bestMove.toRow, bestMove.toCol, captureDestination.first, captureDestination.second, board, targetPiece->getType(), false);
            PathType attackPath = PathAnalyzer::analyzeMovePath(bestMove.fromRow, bestMove.fromCol, bestMove.toRow, bestMove.toCol, board, aiPiece->getType(), false);

            uart_printf("Captured piece path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(capturePath).c_str(),
                       bestMove.toRow, bestMove.toCol + 2, captureDestination.first, captureDestination.second + 2);
            uart_printf("Attacking piece path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(attackPath).c_str(),
                       bestMove.fromRow, bestMove.fromCol + 2, bestMove.toRow, bestMove.toCol + 2);
        } else if (aiPiece) {
            PathType movePath = PathAnalyzer::analyzeMovePath(bestMove.fromRow, bestMove.fromCol, bestMove.toRow, bestMove.toCol, board, aiPiece->getType(), false);
            uart_printf("Movement path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(movePath).c_str(),
                       bestMove.fromRow, bestMove.fromCol + 2, bestMove.toRow, bestMove.toCol + 2);
        }
        currentPlayer = (currentPlayer == White) ? Black : White;
    } else {
        uart_printf("AI move failed!\r\n");
    }
}

void makeMinimaxMove(ChessBoard& board, Color& currentPlayer, int depth) {
    uart_printf("\r\nMinimax AI thinking (depth %d)...\r\n", depth);

    ChessAI ai;
    AIMove bestMove = ai.findBestMove(board, currentPlayer, depth);

    if (bestMove.fromRow == -1) {
        uart_printf("Minimax AI has no valid moves!\r\n");
        return;
    }

    uart_printf("Best move score: %.2f\r\n", bestMove.score);

    ChessPiece* aiPiece = board.getPiece(bestMove.fromRow, bestMove.fromCol);
    ChessPiece* targetPiece = board.getPiece(bestMove.toRow, bestMove.toCol);
    bool isCapture = (targetPiece != nullptr);

    uart_printf("Minimax AI moves from (%d,%d) to (%d,%d)\r\n",
               bestMove.fromRow, bestMove.fromCol + 2,
               bestMove.toRow, bestMove.toCol + 2);

    board.setTurn(currentPlayer);
    if (board.movePiece(bestMove.fromRow, bestMove.fromCol, bestMove.toRow, bestMove.toCol)) {
        if (isCapture && aiPiece && targetPiece) {
            auto captureDestination = PathAnalyzer::findCaptureZoneDestination(board, targetPiece->getColor());
            PathType capturePath = PathAnalyzer::analyzeMovePath(bestMove.toRow, bestMove.toCol, captureDestination.first, captureDestination.second, board, targetPiece->getType(), false);
            PathType attackPath = PathAnalyzer::analyzeMovePath(bestMove.fromRow, bestMove.fromCol, bestMove.toRow, bestMove.toCol, board, aiPiece->getType(), false);

            uart_printf("Captured piece path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(capturePath).c_str(),
                       bestMove.toRow, bestMove.toCol + 2, captureDestination.first, captureDestination.second + 2);
            uart_printf("Attacking piece path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(attackPath).c_str(),
                       bestMove.fromRow, bestMove.fromCol + 2, bestMove.toRow, bestMove.toCol + 2);
        } else if (aiPiece) {
            PathType movePath = PathAnalyzer::analyzeMovePath(bestMove.fromRow, bestMove.fromCol, bestMove.toRow, bestMove.toCol, board, aiPiece->getType(), false);
            uart_printf("Movement path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(movePath).c_str(),
                       bestMove.fromRow, bestMove.fromCol + 2, bestMove.toRow, bestMove.toCol + 2);
        }
        currentPlayer = (currentPlayer == White) ? Black : White;
    } else {
        uart_printf("Minimax AI move failed!\r\n");
    }
}


void detectAndExecuteMove(ChessBoard& board, int row1, int col1, int row2, int col2, Color& currentPlayer) {
    ChessPiece* piece1 = board.getPiece(row1, col1);
    ChessPiece* piece2 = board.getPiece(row2, col2);

    uart_printf("[TEST MODE] Analyzing positions (%d,%d) and (%d,%d)\r\n",
                row1, col1 + 2, row2, col2 + 2);

    if (!piece1 && !piece2) {
        uart_printf("Error: Both positions are empty. No move possible.\r\n");
        return;
    }

    if (piece1 && !piece2) {
        if (piece1->getColor() != currentPlayer) {
            uart_printf("Error: Piece at (%d,%d) belongs to %s, but it's %s's turn.\r\n",
                       row1, col1 + 2, colorToString(piece1->getColor()).c_str(),
                       colorToString(currentPlayer).c_str());
            return;
        }

        uart_printf("Detected: %s %s moves from (%d,%d) to (%d,%d)\r\n",
                   colorToString(piece1->getColor()).c_str(),
                   piece1->getType() == Pawn ? "pawn" :
                   piece1->getType() == Rook ? "rook" :
                   piece1->getType() == Knight ? "knight" :
                   piece1->getType() == Bishop ? "bishop" :
                   piece1->getType() == Queen ? "queen" : "king",
                   row1, col1 + 2, row2, col2 + 2);

        board.setTurn(currentPlayer);
        if (board.movePiece(row1, col1, row2, col2)) {
            PathType movePath = PathAnalyzer::analyzeMovePath(row1, col1, row2, col2, board, piece1->getType(), false);
            currentPlayer = (currentPlayer == White) ? Black : White;
            uart_printf("Move successful!\r\n");
            uart_printf("Movement path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(movePath).c_str(),
                       row1, col1 + 2, row2, col2 + 2);
        } else {
            uart_printf("Error: Invalid move.\r\n");
        }
        return;
    }

    if (!piece1 && piece2) {
        if (piece2->getColor() != currentPlayer) {
            uart_printf("Error: Piece at (%d,%d) belongs to %s, but it's %s's turn.\r\n",
                       row2, col2 + 2, colorToString(piece2->getColor()).c_str(),
                       colorToString(currentPlayer).c_str());
            return;
        }

        uart_printf("Detected: %s %s moves from (%d,%d) to (%d,%d)\r\n",
                   colorToString(piece2->getColor()).c_str(),
                   piece2->getType() == Pawn ? "pawn" :
                   piece2->getType() == Rook ? "rook" :
                   piece2->getType() == Knight ? "knight" :
                   piece2->getType() == Bishop ? "bishop" :
                   piece2->getType() == Queen ? "queen" : "king",
                   row2, col2 + 2, row1, col1 + 2);

        board.setTurn(currentPlayer);
        if (board.movePiece(row2, col2, row1, col1)) {
            PathType movePath = PathAnalyzer::analyzeMovePath(row2, col2, row1, col1, board, piece2->getType(), false);
            currentPlayer = (currentPlayer == White) ? Black : White;
            uart_printf("Move successful!\r\n");
            uart_printf("Movement path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(movePath).c_str(),
                       row2, col2 + 2, row1, col1 + 2);
        } else {
            uart_printf("Error: Invalid move.\r\n");
        }
        return;
    }

    if (piece1 && piece2) {
        Color color1 = piece1->getColor();
        Color color2 = piece2->getColor();

        if (color1 == color2) {
            if (color1 != currentPlayer) {
                uart_printf("Error: Both pieces belong to %s, but it's %s's turn.\r\n",
                           colorToString(color1).c_str(),
                           colorToString(currentPlayer).c_str());
                return;
            }

            if ((piece1->getType() == King && piece2->getType() == Rook) ||
                (piece1->getType() == Rook && piece2->getType() == King)) {
                uart_printf("Detected: Possible castling move\r\n");
                if (piece1->getType() == King) {
                    int kingCol = col1;
                    int rookCol = col2;
                    int newKingCol = (rookCol > kingCol) ? kingCol + 2 : kingCol - 2;

                    board.setTurn(currentPlayer);
                    if (board.movePiece(row1, kingCol, row1, newKingCol)) {
                        PathType castlingPath = PathAnalyzer::analyzeMovePath(row1, kingCol, row1, newKingCol, board, King, true);
                        currentPlayer = (currentPlayer == White) ? Black : White;
                        uart_printf("Castling successful!\r\n");
                        uart_printf("Movement path: %s\r\n", PathAnalyzer::pathTypeToString(castlingPath).c_str());
                    } else {
                        uart_printf("Error: Castling not allowed.\r\n");
                    }
                } else {
                    int kingCol = col2;
                    int rookCol = col1;
                    int newKingCol = (rookCol > kingCol) ? kingCol + 2 : kingCol - 2;

                    board.setTurn(currentPlayer);
                    if (board.movePiece(row2, kingCol, row2, newKingCol)) {
                        PathType castlingPath = PathAnalyzer::analyzeMovePath(row2, kingCol, row2, newKingCol, board, King, true);
                        currentPlayer = (currentPlayer == White) ? Black : White;
                        uart_printf("Castling successful!\r\n");
                        uart_printf("Movement path: %s\r\n", PathAnalyzer::pathTypeToString(castlingPath).c_str());
                    } else {
                        uart_printf("Error: Castling not allowed.\r\n");
                    }
                }
            } else {
                uart_printf("Error: Both pieces are the same color but not King and Rook.\r\n");
            }
            return;
        }

        if (color1 == currentPlayer) {
            uart_printf("Detected: %s %s at (%d,%d) captures %s %s at (%d,%d)\r\n",
                       colorToString(color1).c_str(),
                       piece1->getType() == Pawn ? "pawn" :
                       piece1->getType() == Rook ? "rook" :
                       piece1->getType() == Knight ? "knight" :
                       piece1->getType() == Bishop ? "bishop" :
                       piece1->getType() == Queen ? "queen" : "king",
                       row1, col1 + 2,
                       colorToString(color2).c_str(),
                       piece2->getType() == Pawn ? "pawn" :
                       piece2->getType() == Rook ? "rook" :
                       piece2->getType() == Knight ? "knight" :
                       piece2->getType() == Bishop ? "bishop" :
                       piece2->getType() == Queen ? "queen" : "king",
                       row2, col2 + 2);

            board.setTurn(currentPlayer);
            if (board.movePiece(row1, col1, row2, col2)) {
                auto captureDestination = PathAnalyzer::findCaptureZoneDestination(board, piece2->getColor());
                PathType capturePath = PathAnalyzer::analyzeMovePath(row2, col2, captureDestination.first, captureDestination.second, board, piece2->getType(), false);
                PathType attackPath = PathAnalyzer::analyzeMovePath(row1, col1, row2, col2, board, piece1->getType(), false);

                currentPlayer = (currentPlayer == White) ? Black : White;
                uart_printf("Capture successful!\r\n");
                uart_printf("Captured piece path: %s from (%d,%d) to (%d,%d)\r\n",
                           PathAnalyzer::pathTypeToString(capturePath).c_str(),
                           row2, col2 + 2, captureDestination.first, captureDestination.second + 2);
                uart_printf("Attacking piece path: %s from (%d,%d) to (%d,%d)\r\n",
                           PathAnalyzer::pathTypeToString(attackPath).c_str(),
                           row1, col1 + 2, row2, col2 + 2);
            } else {
                uart_printf("Error: Invalid capture.\r\n");
            }
        } else if (color2 == currentPlayer) {
            uart_printf("Detected: %s %s at (%d,%d) captures %s %s at (%d,%d)\r\n",
                       colorToString(color2).c_str(),
                       piece2->getType() == Pawn ? "pawn" :
                       piece2->getType() == Rook ? "rook" :
                       piece2->getType() == Knight ? "knight" :
                       piece2->getType() == Bishop ? "bishop" :
                       piece2->getType() == Queen ? "queen" : "king",
                       row2, col2 + 2,
                       colorToString(color1).c_str(),
                       piece1->getType() == Pawn ? "pawn" :
                       piece1->getType() == Rook ? "rook" :
                       piece1->getType() == Knight ? "knight" :
                       piece1->getType() == Bishop ? "bishop" :
                       piece1->getType() == Queen ? "queen" : "king",
                       row1, col1 + 2);

            board.setTurn(currentPlayer);
            if (board.movePiece(row2, col2, row1, col1)) {
                auto captureDestination = PathAnalyzer::findCaptureZoneDestination(board, piece1->getColor());
                PathType capturePath = PathAnalyzer::analyzeMovePath(row1, col1, captureDestination.first, captureDestination.second, board, piece1->getType(), false);
                PathType attackPath = PathAnalyzer::analyzeMovePath(row2, col2, row1, col1, board, piece2->getType(), false);

                currentPlayer = (currentPlayer == White) ? Black : White;
                uart_printf("Capture successful!\r\n");
                uart_printf("Captured piece path: %s from (%d,%d) to (%d,%d)\r\n",
                           PathAnalyzer::pathTypeToString(capturePath).c_str(),
                           row1, col1 + 2, captureDestination.first, captureDestination.second + 2);
                uart_printf("Attacking piece path: %s from (%d,%d) to (%d,%d)\r\n",
                           PathAnalyzer::pathTypeToString(attackPath).c_str(),
                           row2, col2 + 2, row1, col1 + 2);
            } else {
                uart_printf("Error: Invalid capture.\r\n");
            }
        } else {
            uart_printf("Error: Neither piece belongs to the current player (%s).\r\n",
                       colorToString(currentPlayer).c_str());
        }
    }
}

void uart_read_line(char* buffer, int max_len) {
    int pos = 0;
    while (pos < max_len - 1) {
        uint8_t c;
        int len = uart_read_bytes(UART_NUM, &c, 1, portMAX_DELAY);
        if (len > 0) {
            uart_write_bytes(UART_NUM, (const char*)&c, 1);

            if (c == '\n' || c == '\r') {
                uart_write_bytes(UART_NUM, "\r\n", 2);
                break;
            } else if (c == '\b' || c == 127) {
                if (pos > 0) {
                    pos--;
                    uart_write_bytes(UART_NUM, "\b \b", 3);
                }
            } else if (c >= 32 && c < 127) {
                buffer[pos++] = c;
            }
        }
    }
    buffer[pos] = '\0';
}

void uart_printf(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    uart_write_bytes(UART_NUM, buffer, strlen(buffer));
}

void chess_game_task(void *pvParameter) {
    vTaskDelay(500 / portTICK_PERIOD_MS);

    ChessBoard board(8, 8);
    setupStandardBoard(board);

    Color currentPlayer = White;
    bool gameRunning = true;
    char input[256];

    uart_printf("\r\n=== ESP32 Chess with Capture Zones ===\r\n");
    uart_printf("Board columns: 0-1 (captured black), 2-9 (playable), A-B (captured white)\r\n");
    uart_printf("Enter moves using columns 2-9:\r\n");
    uart_printf("  1. row col - Select a piece and see possible moves\r\n");
    uart_printf("  2. row1 col1 row2 col2 - Direct move\r\n");
    uart_printf("Example: 6 6 (selects pawn at row 6, column 6)\r\n");
    uart_printf("Commands: quit, help, board, new, ai, minimax [depth], state tracking\r\n\r\n");

    string boardStr = board.displayExpandedBoard().str();
    uart_printf("%s\r\n", boardStr.c_str());

    while (gameRunning) {
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
                uart_printf("\r\nNEW GAME\r\n");
                boardStr = board.displayExpandedBoard().str();
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
                uart_printf("\r\nNEW GAME\r\n");
                boardStr = board.displayExpandedBoard().str();
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
            uart_printf("Board Layout:\r\n");
            uart_printf("  Columns 0-1: Captured black pieces\r\n");
            uart_printf("  Columns 2-9: Main chess board (playable area)\r\n");
            uart_printf("  Columns A-B: Captured white pieces\r\n");
            uart_printf("Move formats:\r\n");
            uart_printf("  row col         - Select piece and see possible moves\r\n");
            uart_printf("  row1 col1 row2 col2 - Direct move\r\n");
            uart_printf("Examples:\r\n");
            uart_printf("  6 6      - Select pawn at row 6, column 6 (display)\r\n");
            uart_printf("  6 6 4 6  - Move from (6,6) to (4,6)\r\n");
            uart_printf("NOTE: Use columns 2-9 for chess moves (not 0-1 or A-B)\r\n");
            uart_printf("Special commands:\r\n");
            uart_printf("  'quit'  - Exit the game\r\n");
            uart_printf("  'help'  - Show this help\r\n");
            uart_printf("  'board' - Redraw the board\r\n");
            uart_printf("  'new'   - Start a new game\r\n");
            uart_printf("  'ai'    - Let AI make a move (Simple AI)\r\n");
            uart_printf("  'minimax [depth]' - Let Minimax AI make a move (depth 1-4, default 2)\r\n");
            uart_printf("  'test'  - Sensor simulation mode (detect move from 2 positions)\r\n");
            uart_printf("  'state tracking' - Replay and validate board states from Board_States folder\r\n");

            continue;
        } else if (strcmp(input, "board") == 0) {
            boardStr = board.displayExpandedBoard().str();
            uart_printf("\r\n%s\r\n", boardStr.c_str());
            continue;
        } else if (strcmp(input, "new") == 0) {
            board = ChessBoard(8, 8);
            setupStandardBoard(board);
            currentPlayer = White;
            uart_printf("\r\n NEW GAME\r\n");
            boardStr = board.displayExpandedBoard().str();
            uart_printf("%s\r\n", boardStr.c_str());
            continue;
        } else if (strcmp(input, "ai") == 0) {
            makeAIMove(board, currentPlayer);
            boardStr = board.displayExpandedBoard().str();
            uart_printf("\r\n%s\r\n", boardStr.c_str());
            continue;
        } else if (strncmp(input, "minimax", 7) == 0) {
            int depth = 2;
            if (strlen(input) > 8) {
                sscanf(input + 8, "%d", &depth);
                if (depth < 1) depth = 1;
                if (depth > 4) depth = 4;
            }
            makeMinimaxMove(board, currentPlayer, depth);
            boardStr = board.displayExpandedBoard().str();
            uart_printf("\r\n%s\r\n", boardStr.c_str());
            continue;
        } else if (strcmp(input, "test") == 0) {
            bool testModeComplete = false;
            while (!testModeComplete) {
                uart_printf("[TEST MODE] Enter two positions (row1 col1 row2 col2): ");
                uart_read_line(input, sizeof(input));

                if (strcmp(input, "cancel") == 0) {
                    uart_printf("Test mode cancelled.\r\n");
                    break;
                }

                int nums[10];
                int parseCount = sscanf(input, "%d %d %d %d %d %d %d %d %d %d",
                                       &nums[0], &nums[1], &nums[2], &nums[3], &nums[4],
                                       &nums[5], &nums[6], &nums[7], &nums[8], &nums[9]);

                if (parseCount < 4) {
                    uart_printf("Error: Please enter at least 4 numbers (row1 col1 row2 col2)\r\n");
                    continue;
                }

                if (parseCount > 4) {
                    int numPieces = (parseCount + 1) / 2;
                    uart_printf("\r\nError! %d pieces picked up! Reset board and try again!\r\n", numPieces);

                    boardStr = board.displayExpandedBoard().str();
                    uart_printf("\r\n%s", boardStr.c_str());

                    continue;
                }

                int testRow1 = nums[0];
                int testCol1 = nums[1];
                int testRow2 = nums[2];
                int testCol2 = nums[3];

                if (testRow1 < 0 || testRow1 > 7 || testRow2 < 0 || testRow2 > 7) {
                    uart_printf("Error: Rows must be between 0 and 7.\r\n");
                    continue;
                }

                int internalCol1 = translateColumn(testCol1);
                int internalCol2 = translateColumn(testCol2);

                if (internalCol1 == -1) {
                    uart_printf("Error: Column %d is not on the playable board. Use columns 2-9.\r\n", testCol1);
                    continue;
                }
                if (internalCol2 == -1) {
                    uart_printf("Error: Column %d is not on the playable board. Use columns 2-9.\r\n", testCol2);
                    continue;
                }

                if (testRow1 == testRow2 && internalCol1 == internalCol2) {
                    uart_printf("Error: Both positions are the same! Please enter two different positions.\r\n");
                    continue;
                }

                detectAndExecuteMove(board, testRow1, internalCol1, testRow2, internalCol2, currentPlayer);

                boardStr = board.displayExpandedBoard().str();
                uart_printf("\r\n%s\r\n", boardStr.c_str());

                testModeComplete = true;
            }
            continue;
        } else if (strcmp(input, "state tracking") == 0) {
            if (NUM_BOARD_STATES == 0) {
                uart_printf("Error: No board state files found.\r\n");
                continue;
            }

            uart_printf("\r\n=== Starting State Tracking Mode ===\r\n");
            uart_printf("Found %d board state files.\r\n\r\n", NUM_BOARD_STATES);

            BoardStateTracker tracker;

            if (!tracker.validateStartingPosition((bool(*)[8])BOARD_STATES[0])) {
                uart_printf("Error: Board_State_0 does not match expected starting position.\r\n");
                tracker.showExpectedStartingPosition();
                uart_printf("Please correct Board_State_0.txt and rebuild.\r\n");
                continue;
            }

            uart_printf("Board_State_0 validated successfully.\r\n");
            uart_printf("Initializing tracking board...\r\n\r\n");

            ChessBoard trackingBoard(8, 8);
            tracker.initializeTracking(trackingBoard, (bool(*)[8])BOARD_STATES[0]);
            Color trackingPlayer = White;

            uart_printf("Initial board state (Board_State_0):\r\n");
            boardStr = trackingBoard.displayExpandedBoard().str();
            uart_printf("%s\r\n", boardStr.c_str());

            int currentStateIndex = 1;
            bool tracking = true;

            while (tracking && currentStateIndex < NUM_BOARD_STATES) {
                uart_printf("Press 'y' to advance to Board_State_%d, 'n' to exit: ", currentStateIndex);
                uart_read_line(input, sizeof(input));

                if (strcmp(input, "n") == 0 || strcmp(input, "N") == 0) {
                    uart_printf("Exiting state tracking mode.\r\n");
                    tracking = false;
                    break;
                }

                if (strcmp(input, "y") == 0 || strcmp(input, "Y") == 0) {
                    uart_printf("\r\nProcessing Board_State_%d...\r\n", currentStateIndex);

                    bool success = tracker.processNextState(
                        trackingBoard,
                        (bool(*)[8])BOARD_STATES[currentStateIndex - 1],
                        (bool(*)[8])BOARD_STATES[currentStateIndex],
                        trackingPlayer
                    );

                    if (success) {
                        uart_printf("\r\nBoard after move:\r\n");
                        boardStr = trackingBoard.displayExpandedBoard().str();
                        uart_printf("%s\r\n", boardStr.c_str());
                        currentStateIndex++;
                    } else {
                        uart_printf("\r\nCurrent valid board state (what your board should look like):\r\n");
                        boardStr = trackingBoard.displayExpandedBoard().str();
                        uart_printf("%s\r\n", boardStr.c_str());
                        uart_printf("Staying on last valid board state. Press 'y' when Board_State_%d is corrected, or 'n' to exit.\r\n", currentStateIndex);
                    }
                } else {
                    uart_printf("Invalid input. Please enter 'y' or 'n'.\r\n");
                }
            }

            if (currentStateIndex >= NUM_BOARD_STATES) {
                uart_printf("\r\n=== All board states processed successfully ===\r\n");
            }

            uart_printf("Returning to normal game mode.\r\n\r\n");
            continue;
        }

        int nums[4];
        int count = sscanf(input, "%d %d %d %d", &nums[0], &nums[1], &nums[2], &nums[3]);

        if (count == 2) {
            int fromRow = nums[0];
            int displayCol = nums[1];
            int fromCol = translateColumn(displayCol);

            if (fromCol == -1) {
                uart_printf("Column %d is not on the playable board. Use columns 2-9.\r\n", displayCol);
                continue;
            }

            ChessPiece* piece = board.getPiece(fromRow, fromCol);
            if (!piece) {
                uart_printf("No piece at position (%d,%d)\r\n", fromRow, displayCol);
                continue;
            }

            if (piece->getColor() != currentPlayer) {
                uart_printf("That's not your piece!\r\n");
                continue;
            }

            std::vector<std::pair<int, int>> possibleMoves = board.getPossibleMoves(fromRow, fromCol);

            if (possibleMoves.empty()) {
                uart_printf("No valid moves for this piece!\r\n");
                continue;
            }

            uart_printf("\r\nPossible moves for piece at (%d,%d):\r\n", fromRow, displayCol);
            for (size_t i = 0; i < possibleMoves.size(); i++) {
                uart_printf("  %d: (%d,%d)\r\n", (int)(i + 1), possibleMoves[i].first, possibleMoves[i].second + 2);
            }

            uart_printf("\r\nEnter move number (1-%d) or 'cancel' to select different piece: ", (int)possibleMoves.size());
            uart_read_line(input, sizeof(input));

            if (strcmp(input, "cancel") == 0) {
                continue;
            }

            int moveChoice;
            if (sscanf(input, "%d", &moveChoice) != 1 || moveChoice < 1 || moveChoice > (int)possibleMoves.size()) {
                uart_printf("Invalid choice!\r\n");
                continue;
            }

            int toRow = possibleMoves[moveChoice - 1].first;
            int toCol = possibleMoves[moveChoice - 1].second;

            ChessPiece* targetPiece = board.getPiece(toRow, toCol);
            if (targetPiece != nullptr) {
                uart_printf("\r\nMOVING CAPTURED PIECE\r\n");

                uart_printf("\r\nMovement complete (y or n)? ");
                uart_read_line(input, sizeof(input));

                if (input[0] != 'y' && input[0] != 'Y') {
                    uart_printf("Capture cancelled.\r\n");
                    continue;
                }

                uart_printf("Executing capture...\r\n");
            }

            ChessPiece* movingPiece = board.getPiece(fromRow, fromCol);
            board.setTurn(currentPlayer);

            if (board.movePiece(fromRow, fromCol, toRow, toCol)) {
                if (isKingInCheck(board, currentPlayer)) {
                    board.setTurn(currentPlayer);
                    board.movePiece(toRow, toCol, fromRow, fromCol);
                    uart_printf("Invalid move: would put your king in check!\r\n");
                    continue;
                }

                if (targetPiece != nullptr && movingPiece) {
                    auto captureDestination = PathAnalyzer::findCaptureZoneDestination(board, targetPiece->getColor());
                    PathType capturePath = PathAnalyzer::analyzeMovePath(toRow, toCol, captureDestination.first, captureDestination.second, board, targetPiece->getType(), false);
                    PathType attackPath = PathAnalyzer::analyzeMovePath(fromRow, fromCol, toRow, toCol, board, movingPiece->getType(), false);

                    uart_printf("Captured piece path: %s from (%d,%d) to (%d,%d)\r\n",
                               PathAnalyzer::pathTypeToString(capturePath).c_str(),
                               toRow, toCol + 2, captureDestination.first, captureDestination.second + 2);
                    uart_printf("Attacking piece path: %s from (%d,%d) to (%d,%d)\r\n",
                               PathAnalyzer::pathTypeToString(attackPath).c_str(),
                               fromRow, fromCol + 2, toRow, toCol + 2);
                } else if (movingPiece) {
                    PathType movePath = PathAnalyzer::analyzeMovePath(fromRow, fromCol, toRow, toCol, board, movingPiece->getType(), false);
                    uart_printf("Movement path: %s from (%d,%d) to (%d,%d)\r\n",
                               PathAnalyzer::pathTypeToString(movePath).c_str(),
                               fromRow, fromCol + 2, toRow, toCol + 2);
                }

                currentPlayer = (currentPlayer == White) ? Black : White;
                boardStr = board.displayExpandedBoard().str();
                uart_printf("\r\n%s\r\n", boardStr.c_str());
            } else {
                uart_printf("Move failed!\r\n");
            }

        } else if (count == 4) {
            int fromRow = nums[0];
            int displayFromCol = nums[1];
            int toRow = nums[2];
            int displayToCol = nums[3];

            int fromCol = translateColumn(displayFromCol);
            int toCol = translateColumn(displayToCol);

            if (fromCol == -1) {
                uart_printf("From column %d is not on the playable board. Use columns 2-9.\r\n", displayFromCol);
                continue;
            }
            if (toCol == -1) {
                uart_printf("To column %d is not on the playable board. Use columns 2-9.\r\n", displayToCol);
                continue;
            }

            ChessPiece* piece = board.getPiece(fromRow, fromCol);
            if (!piece) {
                uart_printf("No piece at position (%d,%d)\r\n", fromRow, displayFromCol);
                continue;
            }

            if (piece->getColor() != currentPlayer) {
                uart_printf("That's not your piece!\r\n");
                continue;
            }

            ChessPiece* targetPiece = board.getPiece(toRow, toCol);
            if (targetPiece != nullptr) {
                uart_printf("\r\nMOVING CAPTURED PIECE\r\n");

                uart_printf("\r\nMovement complete (y or n)? ");
                uart_read_line(input, sizeof(input));

                if (input[0] != 'y' && input[0] != 'Y') {
                    uart_printf("Capture cancelled.\r\n");
                    continue;
                }

                uart_printf("Executing capture...\r\n");
            }

            board.setTurn(currentPlayer);

            if (board.movePiece(fromRow, fromCol, toRow, toCol)) {
                if (isKingInCheck(board, currentPlayer)) {
                    board.setTurn(currentPlayer);
                    board.movePiece(toRow, toCol, fromRow, fromCol);
                    uart_printf("Invalid move: would put your king in check!\r\n");
                    continue;
                }

                if (targetPiece != nullptr && piece) {
                    auto captureDestination = PathAnalyzer::findCaptureZoneDestination(board, targetPiece->getColor());
                    PathType capturePath = PathAnalyzer::analyzeMovePath(toRow, toCol, captureDestination.first, captureDestination.second, board, targetPiece->getType(), false);
                    PathType attackPath = PathAnalyzer::analyzeMovePath(fromRow, fromCol, toRow, toCol, board, piece->getType(), false);

                    uart_printf("Captured piece path: %s from (%d,%d) to (%d,%d)\r\n",
                               PathAnalyzer::pathTypeToString(capturePath).c_str(),
                               toRow, toCol + 2, captureDestination.first, captureDestination.second + 2);
                    uart_printf("Attacking piece path: %s from (%d,%d) to (%d,%d)\r\n",
                               PathAnalyzer::pathTypeToString(attackPath).c_str(),
                               fromRow, fromCol + 2, toRow, toCol + 2);
                } else if (piece) {
                    PathType movePath = PathAnalyzer::analyzeMovePath(fromRow, fromCol, toRow, toCol, board, piece->getType(), false);
                    uart_printf("Movement path: %s from (%d,%d) to (%d,%d)\r\n",
                               PathAnalyzer::pathTypeToString(movePath).c_str(),
                               fromRow, fromCol + 2, toRow, toCol + 2);
                }

                currentPlayer = (currentPlayer == White) ? Black : White;
                boardStr = board.displayExpandedBoard().str();
                uart_printf("\r\n%s\r\n", boardStr.c_str());
            } else {
                uart_printf("Invalid move!\r\n");
            }
        } else {
            uart_printf("Invalid input! Use: row col (to select piece) or row1 col1 row2 col2 (direct move)\r\n");
            continue;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    uart_printf("\r\nGame ended. ESP32 will restart in 5 seconds\r\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    esp_restart();
}

extern "C" {
    void app_main(void);
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting ESP32 Chess Game");

    uart_config_t uart_config = {};
    uart_config.baud_rate = 115200;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 0;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(chess_game_task, "chess_game", 8192, NULL, 10, NULL);
}