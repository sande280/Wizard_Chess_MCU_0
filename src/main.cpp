// General Includes
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
#include <cmath>
#include <cstring>
#include "driver/ledc.h"
#include "esp_timer.h"
#include "driver/i2c.h"
// #include "ota_server.hpp"

//Chess Includes (Jackson)
#include "Chess.h"
#include "ChessBoard.hh"
#include "ChessPiece.hh"
#include "BoardStateTracker.hh"
#include "BoardStates.hh"
#include "PathAnalyzer.hh"
#include "ChessAI.hh"

//Movement Includes (Jack)
#include "move_queue.h"
#include "motionPos.h"
#include <PinDefs.h>
#include "step_timer.h"

//Peripheral Includes (Brayden)
#include "reed.hpp"
#include "leds.hpp"
#include "audio.hpp"
#include "tone.hpp"


#include "path.h"

esp_timer_handle_t step_timer = nullptr;

using namespace Student;
using namespace std;

static const char *TAG = "ESP_CHESS";

#define UART_NUM UART_NUM_0
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)


reed* switches = nullptr;
audio* speaker = nullptr;
leds* led = nullptr;


typedef struct {
    uint32_t total_A, total_B;
    uint32_t sent_A, sent_B;
    int dirA, dirB;
    uint32_t leader_steps;
    uint32_t follower_steps;
    int leader_id; // 1=A, 2=B
    float step_period_us;
    int error_term;
    bool active;
} move_state_t;

static move_state_t move_ctx{};

// Pawn promotion state variables
volatile bool promotionPending = false;
volatile Type promotionChoice = Queen;

void setupStandardBoard(ChessBoard& board);
bool isKingInCheck(ChessBoard& board, Color color);
bool hasValidMoves(ChessBoard& board, Color color);
string colorToString(Color color);
int translateColumn(int displayCol);
void uart_printf(const char* format, ...);
void uart_read_line(char* buffer, int max_len);
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

// Detect if move is castling (king moves 2 squares horizontally)
bool isCastlingMove(ChessBoard& board, int fromRow, int fromCol, int toRow, int toCol) {
    ChessPiece* piece = board.getPiece(fromRow, fromCol);
    if (!piece || piece->getType() != King) return false;
    return (fromRow == toRow && std::abs(toCol - fromCol) == 2);
}

// Detect if move is en passant (pawn diagonal to empty square at enP position)
bool isEnPassantMove(ChessBoard& board, int fromRow, int fromCol, int toRow, int toCol) {
    ChessPiece* piece = board.getPiece(fromRow, fromCol);
    if (!piece || piece->getType() != Pawn) return false;
    // Diagonal move to empty square
    if (board.getPiece(toRow, toCol) != nullptr) return false;
    if (std::abs(toCol - fromCol) != 1) return false;
    // Check if matches en passant target
    return board.isEnPassantTarget(toRow, toCol);
}

int translateColumn(int displayCol) {
    if (displayCol < 2 || displayCol > 9) {
        return -1;
    }
    return displayCol - 2;
}


namespace Student {
    struct SimpleMove {
        int fromRow, fromCol, toRow, toCol;
        int value;
    };
    SimpleMove findBestSimpleMove(ChessBoard& board, Color aiColor);
}

void makeAIMove(ChessBoard& board, Color& currentPlayer) {
    char input[256];
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

            uart_printf("\r\nCaptured piece path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(capturePath).c_str(),
                       bestMove.toRow, bestMove.toCol + 2, captureDestination.first, captureDestination.second + 2);


            do {
                uart_printf("Move captured piece to capture zone - Movement complete (y or n)? ");
                uart_read_line(input, sizeof(input));
            } while (strlen(input) == 0);

            if (input[0] != 'y' && input[0] != 'Y') {
                uart_printf("Move cancelled.\r\n");
                board.setTurn(currentPlayer);
                board.movePiece(bestMove.toRow, bestMove.toCol, bestMove.fromRow, bestMove.fromCol);
                return;
            }

            uart_printf("\r\nAttacking piece path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(attackPath).c_str(),
                       bestMove.fromRow, bestMove.fromCol + 2, bestMove.toRow, bestMove.toCol + 2);


            do {
                uart_printf("Move attacking piece to destination - Movement complete (y or n)? ");
                uart_read_line(input, sizeof(input));
            } while (strlen(input) == 0);

            if (input[0] != 'y' && input[0] != 'Y') {
                uart_printf("Move cancelled.\r\n");
                board.setTurn(currentPlayer);
                board.movePiece(bestMove.toRow, bestMove.toCol, bestMove.fromRow, bestMove.fromCol);
                return;
            }
        } else if (aiPiece) {

            PathType movePath = PathAnalyzer::analyzeMovePath(bestMove.fromRow, bestMove.fromCol, bestMove.toRow, bestMove.toCol, board, aiPiece->getType(), false);
            uart_printf("\r\nMovement path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(movePath).c_str(),
                       bestMove.fromRow, bestMove.fromCol + 2, bestMove.toRow, bestMove.toCol + 2);


            do {
                uart_printf("Movement complete (y or n)? ");
                uart_read_line(input, sizeof(input));
            } while (strlen(input) == 0);

            if (input[0] != 'y' && input[0] != 'Y') {
                uart_printf("Move cancelled.\r\n");
                board.setTurn(currentPlayer);
                board.movePiece(bestMove.toRow, bestMove.toCol, bestMove.fromRow, bestMove.fromCol);
                return;
            }
        }
        currentPlayer = (currentPlayer == White) ? Black : White;
    } else {
        uart_printf("AI move failed!\r\n");
    }
}

void makeMinimaxMove(ChessBoard& board, Color& currentPlayer, int depth) {
    char input[256];
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

            uart_printf("\r\nCaptured piece path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(capturePath).c_str(),
                       bestMove.toRow, bestMove.toCol + 2, captureDestination.first, captureDestination.second + 2);


            do {
                uart_printf("Move captured piece to capture zone - Movement complete (y or n)? ");
                uart_read_line(input, sizeof(input));
            } while (strlen(input) == 0);

            if (input[0] != 'y' && input[0] != 'Y') {
                uart_printf("Move cancelled.\r\n");
                board.setTurn(currentPlayer);
                board.movePiece(bestMove.toRow, bestMove.toCol, bestMove.fromRow, bestMove.fromCol);
                return;
            }

            uart_printf("\r\nAttacking piece path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(attackPath).c_str(),
                       bestMove.fromRow, bestMove.fromCol + 2, bestMove.toRow, bestMove.toCol + 2);


            do {
                uart_printf("Move attacking piece to destination - Movement complete (y or n)? ");
                uart_read_line(input, sizeof(input));
            } while (strlen(input) == 0);

            if (input[0] != 'y' && input[0] != 'Y') {
                uart_printf("Move cancelled.\r\n");
                board.setTurn(currentPlayer);
                board.movePiece(bestMove.toRow, bestMove.toCol, bestMove.fromRow, bestMove.fromCol);
                return;
            }
        } else if (aiPiece) {

            PathType movePath = PathAnalyzer::analyzeMovePath(bestMove.fromRow, bestMove.fromCol, bestMove.toRow, bestMove.toCol, board, aiPiece->getType(), false);
            uart_printf("\r\nMovement path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(movePath).c_str(),
                       bestMove.fromRow, bestMove.fromCol + 2, bestMove.toRow, bestMove.toCol + 2);


            do {
                uart_printf("Movement complete (y or n)? ");
                uart_read_line(input, sizeof(input));
            } while (strlen(input) == 0);

            if (input[0] != 'y' && input[0] != 'Y') {
                uart_printf("Move cancelled.\r\n");
                board.setTurn(currentPlayer);
                board.movePiece(bestMove.toRow, bestMove.toCol, bestMove.fromRow, bestMove.fromCol);
                return;
            }
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
    va_start(args, format);
    // send_to_web_log(format, args); // This will format it again for the web buffer
    va_end(args);
}

#define I2C_SLAVE_SDA_IO        39
#define I2C_SLAVE_SCL_IO        40
#define I2C_SLAVE_PORT          I2C_NUM_1
#define I2C_SLAVE_ADDRESS       0x67
#define I2C_RX_BUF_LEN          256
#define I2C_TX_BUF_LEN          256

// Game state for I2C UI integration
enum GameMode { MODE_NONE, MODE_PHYSICAL, MODE_UI };
static GameMode currentMode = MODE_NONE;
static Color playerColor = White;  // Human player's color
static Color currentTurn = White;
static int selectedRow = -1, selectedCol = -1;

// Previous reed switch state for detecting physical piece pickup
static uint8_t prevReedGrid[12] = {0};
static bool reedGridInitialized = false;

// Flag to signal AI should make a move
static volatile bool aiMoveRequested = false;

// Flags for real-time reed switch LED display (White player, before first move)
static volatile bool showReedStateMode = false;
static volatile bool firstMoveMade = false;
static volatile bool boardSetupComplete = false;  // All 32 starting positions occupied
static volatile bool pieceHeld = false;           // Piece currently being held (lifted from board)

// Capture zone slot tracking
static int capturedWhiteCount = 0;  // White pieces captured, go to rows 10-11
static int capturedBlackCount = 0;  // Black pieces captured, go to rows 0-1

// Get next available capture zone slot for a captured piece
std::pair<int, int> getNextCaptureSlot(Color capturedPieceColor) {
    if (capturedPieceColor == White) {
        // White pieces go to rows 10-11 (max 16 slots)
        int slot = capturedWhiteCount;
        if (slot >= 16) {
            printf("WARNING: White capture zone full, reusing last slot\n");
            slot = 15;  // Reuse last slot if overflow
        } else {
            capturedWhiteCount++;
        }
        int row = 10 + (slot / 8);  // Row 10, then 11
        int col = slot % 8;
        return {row, col};
    } else {
        // Black pieces go to rows 0-1 (max 16 slots)
        int slot = capturedBlackCount;
        if (slot >= 16) {
            printf("WARNING: Black capture zone full, reusing last slot\n");
            slot = 15;  // Reuse last slot if overflow
        } else {
            capturedBlackCount++;
        }
        int row = slot / 8;  // Row 0, then 1
        int col = slot % 8;
        return {row, col};
    }
}

// Wait for all starting pieces to be placed on the board
void waitForBoardSetup() {
    printf("Waiting for board setup - place all pieces...\n");

    // Track which squares are occupied (32 starting squares)
    // rows 0,1 = chess rows 0,1 (black), rows 2,3 = chess rows 6,7 (white)
    bool occupied[4][8] = {{false}};
    int totalOccupied = 0;

    // Clear all LEDs first
    if (led) led->clearPossibleMoves();

    while (totalOccupied < 32) {
        switches->read_matrix();

        // Check chess rows 0, 1 (black pieces)
        for (int chessRow = 0; chessRow < 2; chessRow++) {
            for (int chessCol = 0; chessCol < 8; chessCol++) {
                int idx = chessRow;  // 0 or 1
                if (!occupied[idx][chessCol]) {
                    int physRow = 9 - chessCol;
                    int physCol = chessRow;
                    if (switches->grid[physRow] & (1 << physCol)) {
                        occupied[idx][chessCol] = true;
                        totalOccupied++;
                        if (led) {
                            led->update_led(physRow, physCol, 0, 255, 0);  // Green
                            led->refresh();
                        }
                        printf("Piece placed: chess(%d,%d) -> %d/32\n",
                               chessRow, chessCol, totalOccupied);
                    }
                }
            }
        }

        // Check chess rows 6, 7 (white pieces)
        for (int chessRow = 6; chessRow < 8; chessRow++) {
            for (int chessCol = 0; chessCol < 8; chessCol++) {
                int idx = chessRow - 4;  // 2 or 3
                if (!occupied[idx][chessCol]) {
                    int physRow = 9 - chessCol;
                    int physCol = chessRow;
                    if (switches->grid[physRow] & (1 << physCol)) {
                        occupied[idx][chessCol] = true;
                        totalOccupied++;
                        if (led) {
                            led->update_led(physRow, physCol, 0, 255, 0);  // Green
                            led->refresh();
                        }
                        printf("Piece placed: chess(%d,%d) -> %d/32\n",
                               chessRow, chessCol, totalOccupied);
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    printf("Board setup complete!\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    if (led) led->clearPossibleMoves();  // Turn off LEDs
}

// Check if all 32 starting positions are occupied (non-blocking check)
bool allStartingPositionsOccupied() {
    if (switches == nullptr) return false;

    int count = 0;

    // Check chess rows 0, 1 (black pieces)
    for (int chessRow = 0; chessRow < 2; chessRow++) {
        for (int chessCol = 0; chessCol < 8; chessCol++) {
            int physRow = 9 - chessCol;
            int physCol = chessRow;
            if (switches->grid[physRow] & (1 << physCol)) {
                count++;
            }
        }
    }

    // Check chess rows 6, 7 (white pieces)
    for (int chessRow = 6; chessRow < 8; chessRow++) {
        for (int chessCol = 0; chessCol < 8; chessCol++) {
            int physRow = 9 - chessCol;
            int physCol = chessRow;
            if (switches->grid[physRow] & (1 << physCol)) {
                count++;
            }
        }
    }

    return count >= 32;
}

static void i2c_slave_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_SLAVE,
        .sda_io_num = I2C_SLAVE_SDA_IO,
        .scl_io_num = I2C_SLAVE_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .slave = {
            .addr_10bit_en = 0,
            .slave_addr = I2C_SLAVE_ADDRESS,
            .maximum_speed = 0,
        },
        .clk_flags = 0,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_SLAVE_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_SLAVE_PORT, conf.mode, I2C_RX_BUF_LEN, I2C_TX_BUF_LEN, 0));
}

// Convert ChessPiece to I2C protocol nibble (0-C)
// Protocol: 0=empty, 1-6=black pieces, 7-C=white pieces
uint8_t pieceToNibble(ChessPiece* piece) {
    if (!piece) return 0;
    // Type enum: Pawn=0, Rook=1, Bishop=2, King=3, Knight=4, Queen=5
    // Protocol: pawn=1, bishop=2, knight=3, rook=4, queen=5, king=6
    static const uint8_t typeMap[] = {1, 4, 2, 6, 3, 5};  // Pawn,Rook,Bishop,King,Knight,Queen
    uint8_t base = typeMap[piece->getType()];
    return (piece->getColor() == White) ? base + 6 : base;
}

// Serialize board state to 33-byte buffer (0xAA header + 32 bytes)
void serializeBoardState(ChessBoard& board, uint8_t* buffer) {
    buffer[0] = 0xAA;  // Header
    for (int i = 0; i < 32; i++) {
        int idx = i * 2;
        int row1 = idx / 8, col1 = idx % 8;
        int row2 = (idx + 1) / 8, col2 = (idx + 1) % 8;
        uint8_t high = pieceToNibble(board.getPiece(row1, col1));
        uint8_t low = pieceToNibble(board.getPiece(row2, col2));
        buffer[1 + i] = (high << 4) | low;
    }
}

// Serialize board state with game-over preamble (0x2F instead of 0xAA)
void serializeBoardStateGameOver(ChessBoard& board, uint8_t* buffer) {
    buffer[0] = 0x2F;  // Game-over header
    for (int i = 0; i < 32; i++) {
        int idx = i * 2;
        int row1 = idx / 8, col1 = idx % 8;
        int row2 = (idx + 1) / 8, col2 = (idx + 1) % 8;
        uint8_t high = pieceToNibble(board.getPiece(row1, col1));
        uint8_t low = pieceToNibble(board.getPiece(row2, col2));
        buffer[1 + i] = (high << 4) | low;
    }
}

// Serialize board state flipped for black player perspective (0xAA header + 32 bytes)
// Row 7 becomes row 0, row 6 becomes row 1, etc. Same for columns.
void serializeBoardStateFlipped(ChessBoard& board, uint8_t* buffer) {
    buffer[0] = 0xAA;  // Header
    for (int i = 0; i < 32; i++) {
        int idx = i * 2;
        int row1 = 7 - (idx / 8), col1 = 7 - (idx % 8);
        int row2 = 7 - ((idx + 1) / 8), col2 = 7 - ((idx + 1) % 8);
        uint8_t high = pieceToNibble(board.getPiece(row1, col1));
        uint8_t low = pieceToNibble(board.getPiece(row2, col2));
        buffer[1 + i] = (high << 4) | low;
    }
}

// Serialize board state flipped with game-over preamble (0x2F header)
void serializeBoardStateFlippedGameOver(ChessBoard& board, uint8_t* buffer) {
    buffer[0] = 0x2F;  // Game-over header
    for (int i = 0; i < 32; i++) {
        int idx = i * 2;
        int row1 = 7 - (idx / 8), col1 = 7 - (idx % 8);
        int row2 = 7 - ((idx + 1) / 8), col2 = 7 - ((idx + 1) % 8);
        uint8_t high = pieceToNibble(board.getPiece(row1, col1));
        uint8_t low = pieceToNibble(board.getPiece(row2, col2));
        buffer[1 + i] = (high << 4) | low;
    }
}

// Serialize board state with stalemate preamble (0xEE)
void serializeBoardStateStalemate(ChessBoard& board, uint8_t* buffer) {
    buffer[0] = 0xEE;  // Stalemate header
    for (int i = 0; i < 32; i++) {
        int idx = i * 2;
        int row1 = idx / 8, col1 = idx % 8;
        int row2 = (idx + 1) / 8, col2 = (idx + 1) % 8;
        uint8_t high = pieceToNibble(board.getPiece(row1, col1));
        uint8_t low = pieceToNibble(board.getPiece(row2, col2));
        buffer[1 + i] = (high << 4) | low;
    }
}

// Serialize board state flipped with stalemate preamble (0xEE)
void serializeBoardStateFlippedStalemate(ChessBoard& board, uint8_t* buffer) {
    buffer[0] = 0xEE;  // Stalemate header
    for (int i = 0; i < 32; i++) {
        int idx = i * 2;
        int row1 = 7 - (idx / 8), col1 = 7 - (idx % 8);
        int row2 = 7 - ((idx + 1) / 8), col2 = 7 - ((idx + 1) % 8);
        uint8_t high = pieceToNibble(board.getPiece(row1, col1));
        uint8_t low = pieceToNibble(board.getPiece(row2, col2));
        buffer[1 + i] = (high << 4) | low;
    }
}

// Serialize possible moves to 33-byte buffer (0xDD header + 32 bytes bitmap)
// Each nibble: 0xF = valid move destination, 0x0 = not valid
// Order: Row 0 first (squares 0-7), then Row 1, etc.
void serializePossibleMoves(ChessBoard& board, int row, int col, uint8_t* buffer) {
    buffer[0] = 0xDD;  // Possible moves header
    memset(buffer + 1, 0, 32);  // Clear data bytes
    auto moves = board.getPossibleMoves(row, col);
    for (auto& move : moves) {
        int index = move.first * 8 + move.second;  // 0-63
        int byteIndex = index / 2;
        if (index % 2 == 0) {
            buffer[1 + byteIndex] |= 0xF0;  // High nibble
        } else {
            buffer[1 + byteIndex] |= 0x0F;  // Low nibble
        }
    }
}

// Serialize possible moves flipped for black player perspective
// Positions are flipped so they appear correct from black's viewpoint
void serializePossibleMovesFlipped(ChessBoard& board, int row, int col, uint8_t* buffer) {
    buffer[0] = 0xDD;  // Possible moves header
    memset(buffer + 1, 0, 32);  // Clear data bytes
    auto moves = board.getPossibleMoves(row, col);
    for (auto& move : moves) {
        // Flip the position for black's perspective
        int flippedRow = 7 - move.first;
        int flippedCol = 7 - move.second;
        int index = flippedRow * 8 + flippedCol;  // 0-63
        int byteIndex = index / 2;
        if (index % 2 == 0) {
            buffer[1 + byteIndex] |= 0xF0;  // High nibble
        } else {
            buffer[1 + byteIndex] |= 0x0F;  // Low nibble
        }
    }
}

// Serialize board state with promotion preamble (0xCC header + 32 bytes)
void serializeBoardStatePromotion(ChessBoard& board, uint8_t* buffer) {
    buffer[0] = 0xCC;  // Promotion header
    for (int i = 0; i < 32; i++) {
        int idx = i * 2;
        int row1 = idx / 8, col1 = idx % 8;
        int row2 = (idx + 1) / 8, col2 = (idx + 1) % 8;
        uint8_t high = pieceToNibble(board.getPiece(row1, col1));
        uint8_t low = pieceToNibble(board.getPiece(row2, col2));
        buffer[1 + i] = (high << 4) | low;
    }
}

// Wait for UI to send promotion piece choice via I2C
// Returns the selected piece type (defaults to Queen on timeout)
Type waitForPromotionChoice(ChessBoard& board, uint32_t timeout_ms) {
    // Send promotion request with board state
    uint8_t tx_buffer[33];
    serializeBoardStatePromotion(board, tx_buffer);
    i2c_reset_tx_fifo(I2C_SLAVE_PORT);
    i2c_slave_write_buffer(I2C_SLAVE_PORT, tx_buffer, 33, pdMS_TO_TICKS(100));
    printf("Promotion request sent (0xCC), waiting for UI response...\n");

    promotionPending = true;
    promotionChoice = Queen;  // Default to Queen

    uint32_t start = xTaskGetTickCount();
    while (promotionPending && (xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (promotionPending) {
        printf("Promotion timeout - defaulting to Queen\n");
        promotionPending = false;
    }

    return promotionChoice;
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


        do {
            uart_printf("Enter move: ");
            uart_read_line(input, sizeof(input));
        } while (strlen(input) == 0);

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


            do {
                uart_printf("\r\nEnter move number (1-%d) or 'cancel' to select different piece: ", (int)possibleMoves.size());
                uart_read_line(input, sizeof(input));
            } while (strlen(input) == 0);

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

                    uart_printf("\r\nCaptured piece path: %s from (%d,%d) to (%d,%d)\r\n",
                               PathAnalyzer::pathTypeToString(capturePath).c_str(),
                               toRow, toCol + 2, captureDestination.first, captureDestination.second + 2);


                    do {
                        uart_printf("Move captured piece to capture zone - Movement complete (y or n)? ");
                        uart_read_line(input, sizeof(input));
                    } while (strlen(input) == 0);

                    if (input[0] != 'y' && input[0] != 'Y') {
                        uart_printf("Move cancelled.\r\n");
                        board.setTurn(currentPlayer);
                        board.movePiece(toRow, toCol, fromRow, fromCol);
                        continue;
                    }

                    uart_printf("\r\nAttacking piece path: %s from (%d,%d) to (%d,%d)\r\n",
                               PathAnalyzer::pathTypeToString(attackPath).c_str(),
                               fromRow, fromCol + 2, toRow, toCol + 2);


                    do {
                        uart_printf("Move attacking piece to destination - Movement complete (y or n)? ");
                        uart_read_line(input, sizeof(input));
                    } while (strlen(input) == 0);

                    if (input[0] != 'y' && input[0] != 'Y') {
                        uart_printf("Move cancelled.\r\n");
                        board.setTurn(currentPlayer);
                        board.movePiece(toRow, toCol, fromRow, fromCol);
                        continue;
                    }
                } else if (movingPiece) {

                    PathType movePath = PathAnalyzer::analyzeMovePath(fromRow, fromCol, toRow, toCol, board, movingPiece->getType(), false);
                    uart_printf("\r\nMovement path: %s from (%d,%d) to (%d,%d)\r\n",
                               PathAnalyzer::pathTypeToString(movePath).c_str(),
                               fromRow, fromCol + 2, toRow, toCol + 2);


                    do {
                        uart_printf("Movement complete (y or n)? ");
                        uart_read_line(input, sizeof(input));
                    } while (strlen(input) == 0);

                    if (input[0] != 'y' && input[0] != 'Y') {
                        uart_printf("Move cancelled.\r\n");
                        board.setTurn(currentPlayer);
                        board.movePiece(toRow, toCol, fromRow, fromCol);
                        continue;
                    }
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

                    uart_printf("\r\nCaptured piece path: %s from (%d,%d) to (%d,%d)\r\n",
                               PathAnalyzer::pathTypeToString(capturePath).c_str(),
                               toRow, toCol + 2, captureDestination.first, captureDestination.second + 2);


                    do {
                        uart_printf("Move captured piece to capture zone - Movement complete (y or n)? ");
                        uart_read_line(input, sizeof(input));
                    } while (strlen(input) == 0);

                    if (input[0] != 'y' && input[0] != 'Y') {
                        uart_printf("Move cancelled.\r\n");
                        board.setTurn(currentPlayer);
                        board.movePiece(toRow, toCol, fromRow, fromCol);
                        continue;
                    }

                    uart_printf("\r\nAttacking piece path: %s from (%d,%d) to (%d,%d)\r\n",
                               PathAnalyzer::pathTypeToString(attackPath).c_str(),
                               fromRow, fromCol + 2, toRow, toCol + 2);


                    do {
                        uart_printf("Move attacking piece to destination - Movement complete (y or n)? ");
                        uart_read_line(input, sizeof(input));
                    } while (strlen(input) == 0);

                    if (input[0] != 'y' && input[0] != 'Y') {
                        uart_printf("Move cancelled.\r\n");
                        board.setTurn(currentPlayer);
                        board.movePiece(toRow, toCol, fromRow, fromCol);
                        continue;
                    }
                } else if (piece) {

                    PathType movePath = PathAnalyzer::analyzeMovePath(fromRow, fromCol, toRow, toCol, board, piece->getType(), false);
                    uart_printf("\r\nMovement path: %s from (%d,%d) to (%d,%d)\r\n",
                               PathAnalyzer::pathTypeToString(movePath).c_str(),
                               fromRow, fromCol + 2, toRow, toCol + 2);


                    do {
                        uart_printf("Movement complete (y or n)? ");
                        uart_read_line(input, sizeof(input));
                    } while (strlen(input) == 0);

                    if (input[0] != 'y' && input[0] != 'Y') {
                        uart_printf("Move cancelled.\r\n");
                        board.setTurn(currentPlayer);
                        board.movePiece(toRow, toCol, fromRow, fromCol);
                        continue;
                    }
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


inline void pulse_step(gpio_num_t pin) {
    gpio_set_level(pin, 1);
    uint64_t t0 = esp_timer_get_time();
    while (esp_timer_get_time() - t0 < 2) { }
    gpio_set_level(pin, 0);
}

static void IRAM_ATTR step_timer_cb(void* arg) {
    if (!move_ctx.active) return;

    // Check bounds before stepping
    long next_A = motors.A_pos;
    long next_B = motors.B_pos;
    
    if (!gantry.home_active) {

        if (move_ctx.leader_id == 1) {
            if (move_ctx.sent_A < move_ctx.total_A) {
                next_A += (move_ctx.dirA > 0 ? 1 : -1);
                int temp_err = move_ctx.error_term + move_ctx.follower_steps;
                if (temp_err >= (int)move_ctx.leader_steps) {
                    if (move_ctx.sent_B < move_ctx.total_B) {
                        next_B += (move_ctx.dirB > 0 ? 1 : -1);
                    }
                }
            }
        } else {
            if (move_ctx.sent_B < move_ctx.total_B) {
                next_B += (move_ctx.dirB > 0 ? 1 : -1);
                int temp_err = move_ctx.error_term + move_ctx.follower_steps;
                if (temp_err >= (int)move_ctx.leader_steps) {
                    if (move_ctx.sent_A < move_ctx.total_A) {
                        next_A += (move_ctx.dirA > 0 ? 1 : -1);
                    }
                }
            }
        }

        float next_x = (next_A + next_B) / (2.0f * STEPS_PER_MM);
        float next_y = (next_A - next_B) / (2.0f * STEPS_PER_MM);

        if (next_x < 0.0f || next_y < 0.0f || next_x > 410.0f || next_y > 280.0f) {
            move_ctx.active = false;
            esp_timer_stop(step_timer);
            gantry.motion_active = false;
            gantry.position_reached = true;
            ESP_LOGI(TAG, "Movement stopped: out of bounds (X: %.2f mm, Y: %.2f mm)", next_x, next_y);
            return;
        }

    }

    if (move_ctx.leader_id == 1) {
        if (move_ctx.sent_A < move_ctx.total_A) {
            pulse_step(STEP1_PIN);
            motors.A_pos += (move_ctx.dirA > 0 ? 1 : -1);
            move_ctx.sent_A++;
            move_ctx.error_term += move_ctx.follower_steps;
            if (move_ctx.error_term >= (int)move_ctx.leader_steps) {
                move_ctx.error_term -= move_ctx.leader_steps;
                if (move_ctx.sent_B < move_ctx.total_B) {
                    pulse_step(STEP2_PIN);
                    motors.B_pos += (move_ctx.dirB > 0 ? 1 : -1);
                    move_ctx.sent_B++;
                }
            }
        }
    } else { // B is leader
        if (move_ctx.sent_B < move_ctx.total_B) {
            pulse_step(STEP2_PIN);
            motors.B_pos += (move_ctx.dirB > 0 ? 1 : -1);
            move_ctx.sent_B++;
            move_ctx.error_term += move_ctx.follower_steps;
            if (move_ctx.error_term >= (int)move_ctx.leader_steps) {
                move_ctx.error_term -= move_ctx.leader_steps;
                if (move_ctx.sent_A < move_ctx.total_A) {
                    pulse_step(STEP1_PIN);
                    motors.A_pos += (move_ctx.dirA > 0 ? 1 : -1);
                    move_ctx.sent_A++;
                }
            }
        }
    }

    // stop when both complete
    if (move_ctx.sent_A >= move_ctx.total_A && move_ctx.sent_B >= move_ctx.total_B) {
        move_ctx.active = false;
        esp_timer_stop(step_timer);
        gantry.motion_active = false;
        gantry.position_reached = true;
    }
    
    // Update live XY coordinates
    gantry.x = (motors.A_pos + motors.B_pos) / (2.0f * STEPS_PER_MM);
    gantry.y = (motors.A_pos - motors.B_pos) / (2.0f * STEPS_PER_MM);

}

void gpio_output_init(gpio_num_t pin) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&cfg);
}

void gpio_input_init(gpio_num_t pin, gpio_int_type_t intr_type) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = intr_type
    };
    gpio_config(&cfg);
}


bool moveToXY(float x_target_mm, float y_target_mm, float speed_mm_s, float overshoot, bool magnet_on) {
    
    
    gpio_set_level(MAGNET_PIN, magnet_on);

    if (speed_mm_s <= 0.0f) return false;


    float dx = x_target_mm - gantry.x;
    float dy = y_target_mm - gantry.y;
    float distance = sqrtf(dx*dx + dy*dy);

    if (overshoot > 0.0f) {
        float overshoot_ratio = overshoot / distance;
        x_target_mm += dx * overshoot_ratio;
        y_target_mm += dy * overshoot_ratio;
        distance += overshoot;
    }

    if (distance <= 0.0f) return false;

    float time_s = distance / speed_mm_s;

    long newA = lroundf((x_target_mm + y_target_mm) * STEPS_PER_MM);
    long newB = lroundf((x_target_mm - y_target_mm) * STEPS_PER_MM);
    long deltaA = newA - motors.A_pos;
    long deltaB = newB - motors.B_pos;

    move_ctx.dirA = (deltaA >= 0) ? 1 : -1;
    move_ctx.dirB = (deltaB >= 0) ? 1 : -1;

    move_ctx.total_A = abs(deltaA);
    move_ctx.total_B = abs(deltaB);
    move_ctx.sent_A = move_ctx.sent_B = 0;
    move_ctx.error_term = 0;

    // Set directions
    gpio_set_level(DIR1_PIN, move_ctx.dirA > 0 ? 1 : 0);
    gpio_set_level(DIR2_PIN, move_ctx.dirB > 0 ? 1 : 0);

    // Determine leader/follower
    if (move_ctx.total_A >= move_ctx.total_B) {
        move_ctx.leader_id = 1;
        move_ctx.leader_steps = move_ctx.total_A;
        move_ctx.follower_steps = move_ctx.total_B;
    } else {
        move_ctx.leader_id = 2;
        move_ctx.leader_steps = move_ctx.total_B;
        move_ctx.follower_steps = move_ctx.total_A;
    }

    // step frequency based on leader
    float freq = move_ctx.leader_steps / time_s;
    move_ctx.step_period_us = 1e6 / freq;

    move_ctx.active = true;
    gantry.motion_active = true;
    gantry.position_reached = false;

    gantry.x_target = x_target_mm;
    gantry.y_target = y_target_mm;
    motors.A_target = newA;
    motors.B_target = newB;

    if (!step_timer) {
        esp_timer_create_args_t args = {
            .callback = &step_timer_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "step_timer",
            .skip_unhandled_events = false
        };
        esp_timer_create(&args, &step_timer);
    }

    esp_timer_start_periodic(step_timer, (uint64_t)move_ctx.step_period_us);

    ESP_LOGI("MOVE", "Straight move: freq=%.1f Hz, period=%.1f us, A=%lu B=%lu", freq, move_ctx.step_period_us, move_ctx.total_A, move_ctx.total_B);
    return true;
}

void moveDispatchTask(void *pvParameters) {
    while (true) {
        // If idle and there are queued moves, dispatch the next one
        if (gantry.position_reached && !move_queue_is_empty()) {
            MoveCommand next;
            if (move_queue_pop(&next)) {
                ESP_LOGI("MOVE", "Dispatching queued move to (%.2f, %.2f) speed=%.1f overshoot=%.1f magnet=%d", next.x, next.y, next.speed, next.overshoot, next.magnet ? 1 : 0);
                moveToXY(next.x, next.y, next.speed, next.overshoot, next.magnet);
            }
            else {
                ESP_LOGI("INIT", "Pop failed unexpectedly");
            }
        }

        if (gantry.position_reached) {
            // ESP_LOGI("MOVE", "Idle: position reached");  // Too verbose
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

volatile bool limit_x_triggered = false;
volatile bool limit_y_triggered = false;

static void IRAM_ATTR limit_switch_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    if (gpio_num == LIMIT_Y_PIN) {
        limit_y_triggered = true;
    } else if (gpio_num == LIMIT_X_PIN) {
        limit_x_triggered = true;
    }
    // Stop motors immediately
    move_ctx.active = false;
    esp_timer_stop(step_timer);
    gantry.motion_active = false;
}

// Task to detect physical piece pickup via reed switches and show possible moves on LEDs
void piecePickupDetectionTask(void *pvParameter) {
    ChessBoard* boardPtr = (ChessBoard*)pvParameter;

    // Wait for initial reed switch scan to populate grid
    vTaskDelay(pdMS_TO_TICKS(500));

    printf("piecePickupDetectionTask started, currentMode=%d\n", currentMode);

    while (1) {
        // Read current reed switch state
        switches->read_matrix();

        // Real-time reed switch LED display (before first move)
        // Only update if no piece is being held (to avoid overwriting BLUE/GREEN pickup display)
        if (showReedStateMode && !firstMoveMade && !pieceHeld && led != nullptr) {
            if (!boardSetupComplete) {
                // Show RED for occupied squares during board setup
                led->showBoardSetupState(switches->grid);

                // Check if all starting positions are now occupied
                if (allStartingPositionsOccupied()) {
                    boardSetupComplete = true;
                    printf("Board setup complete! All 32 pieces detected. Game can begin.\n");
                    // Show GREEN to indicate ready
                    led->showBoardReady();
                }
            }
            // When boardSetupComplete but not firstMoveMade, keep GREEN displayed
            // (showBoardReady was called once, LEDs stay that way until piece pickup)
        }

        // Maintain WHITE ambient during gameplay when no piece is held/selected
        // This ensures the white lights stay on between turns in BOTH physical and UI modes
        // Uses reed switch readings so only squares with detected pieces light up
        // Don't refresh if a piece is selected (selectedRow >= 0) - keep showing possible moves
        if ((currentMode == MODE_PHYSICAL || currentMode == MODE_UI) && firstMoveMade && !pieceHeld && selectedRow < 0 && led != nullptr) {
            static int ambientRefreshCounter = 0;
            if (++ambientRefreshCounter >= 5) {  // Refresh every ~500ms (5 * 100ms) for responsiveness
                ambientRefreshCounter = 0;
                led->showAmbientWhiteFromReed(switches->grid);
            }
        }

        // Initialize previous state on first run
        if (!reedGridInitialized) {
            memcpy(prevReedGrid, switches->grid, 12);
            reedGridInitialized = true;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Only process in physical mode when board setup complete AND it's player's turn
        // This prevents detecting AI's physical piece movements as player moves
        if (currentMode == MODE_PHYSICAL && boardSetupComplete && currentTurn == playerColor) {
            // Debug: periodic state output
            static int pickupDebugCounter = 0;
            if (++pickupDebugCounter >= 30) {  // Every 3 seconds
                pickupDebugCounter = 0;
                printf("Pickup state: selRow=%d turn=%d playerCol=%d pieceHeld=%d\n",
                       selectedRow, currentTurn, playerColor, pieceHeld ? 1 : 0);
            }

            // Check main board area (physical rows 2-9 = chess cols 7-0)
            // Coordinate mapping (matching LEDs): physRow = 9 - chessCol, physCol = chessRow
            for (int physRow = 2; physRow < 10; physRow++) {
                int chessCol = 9 - physRow;  // Physical row maps to chess column (inverted)
                uint8_t prev = prevReedGrid[physRow];
                uint8_t curr = switches->grid[physRow];

                // Detect piece lifted: was present (1), now absent (0)
                // Only detect if no piece is currently selected
                uint8_t lifted = prev & ~curr;

                if (lifted && selectedRow < 0) {
                    for (int physCol = 0; physCol < 8; physCol++) {
                        if (lifted & (1 << physCol)) {
                            int chessRow = physCol;  // Physical column maps to chess row
                            printf("Physical pickup detected at chess (%d, %d)\n", chessRow, chessCol);

                            // Verify it's the current player's piece
                            ChessPiece* piece = boardPtr->getPiece(chessRow, chessCol);
                            if (piece && piece->getColor() == currentTurn) {
                                auto moves = boardPtr->getPossibleMoves(chessRow, chessCol);

                                // Mark piece as held to prevent LED overwriting
                                pieceHeld = true;

                                if (led != nullptr && !moves.empty()) {
                                    // Show BLUE on source square, GREEN on valid destinations
                                    led->showPiecePickup(chessRow, chessCol, moves);
                                }

                                selectedRow = chessRow;
                                selectedCol = chessCol;
                                printf("Selected piece for physical move\n");
                            } else {
                                // Debug: explain why piece wasn't selected
                                printf("Pickup rejected: piece=%p, currentTurn=%d, pieceColor=%d\n",
                                       piece, currentTurn, piece ? piece->getColor() : -1);
                            }
                        }
                    }
                }

                // Detect piece placed: was absent (0), now present (1)
                uint8_t placed = ~prev & curr;

                if (placed && selectedRow >= 0) {
                    for (int physCol = 0; physCol < 8; physCol++) {
                        if (placed & (1 << physCol)) {
                            int destRow = physCol;      // Physical column = chess row
                            int destCol = chessCol;     // Already calculated from physRow

                            printf("Physical placement detected at (%d, %d), selected was (%d, %d)\n",
                                   destRow, destCol, selectedRow, selectedCol);

                            // Check if piece was placed back on its original square (cancelled move)
                            if (destRow == selectedRow && destCol == selectedCol) {
                                printf("Piece returned to original square - move cancelled\n");
                                pieceHeld = false;
                                selectedRow = selectedCol = -1;

                                // Restore ambient LEDs
                                if (led != nullptr) {
                                    led->clearPossibleMoves();
                                    if (firstMoveMade) {
                                        led->showAmbientWhiteFromReed(switches->grid);
                                    } else {
                                        led->showBoardReady();
                                    }
                                }
                                continue;  // Don't process as a move
                            }

                            // Piece is no longer held
                            pieceHeld = false;

                            // Validate move is in possible moves list before processing
                            auto possibleMoves = boardPtr->getPossibleMoves(selectedRow, selectedCol);
                            bool moveIsValid = false;
                            for (const auto& move : possibleMoves) {
                                if (move.first == destRow && move.second == destCol) {
                                    moveIsValid = true;
                                    break;
                                }
                            }

                            if (!moveIsValid) {
                                printf("Invalid destination (%d,%d) - not in possible moves\n", destRow, destCol);
                                // Keep selection active so player can try again
                                // But mark piece as not held since it's on the board
                                if (led != nullptr) {
                                    // Re-show possible moves for the selected piece
                                    led->showPiecePickup(selectedRow, selectedCol, possibleMoves);
                                }
                                continue;  // Don't clear selection, let player try again
                            }

                            // Check if this is a capture move - enemy piece at destination
                            ChessPiece* targetPiece = boardPtr->getPiece(destRow, destCol);
                            if (targetPiece != nullptr) {
                                // This is a capture - in physical mode, player handles it manually
                                // Just increment the capture counter (getNextCaptureSlot does this)
                                Color capturedColor = targetPiece->getColor();
                                auto [capZoneRow, capZoneCol] = getNextCaptureSlot(capturedColor);

                                printf("Capture detected! Player manually moved %s piece to capture zone\n",
                                       capturedColor == White ? "White" : "Black");

                                // NO motor movement - player physically moves captured piece themselves
                            }

                            boardPtr->setTurn(currentTurn);
                            if (boardPtr->movePiece(selectedRow, selectedCol, destRow, destCol)) {
                                printf("Physical move successful: (%d,%d) -> (%d,%d)\n",
                                       selectedRow, selectedCol, destRow, destCol);

                                // Re-sync board_state for pathfinding after player move
                                setupMoveTracking(boardPtr);

                                // Mark first move as made and show WHITE ambient
                                if (!firstMoveMade) {
                                    firstMoveMade = true;
                                    printf("First move made - transitioning to WHITE ambient\n");
                                }

                                // Show WHITE ambient under all pieces
                                if (led != nullptr) {
                                    led->showAmbientWhite(*boardPtr);
                                }

                                currentTurn = (currentTurn == White) ? Black : White;

                                // Signal AI to respond if it's now computer's turn
                                if (currentTurn != playerColor) {
                                    aiMoveRequested = true;
                                    printf("AI move requested\n");
                                }

                                selectedRow = selectedCol = -1;
                            } else {
                                printf("Physical move failed unexpectedly\n");
                                // Keep selection so player can try again
                                if (led != nullptr) {
                                    led->showPiecePickup(selectedRow, selectedCol, possibleMoves);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Detect capture zone placements (for debug logging only)
        // Note: Counter increments are handled in getNextCaptureSlot(), not here
        // This detection fires spuriously when the electromagnet passes through
        for (int physRow = 0; physRow < 2; physRow++) {
            uint8_t prev = prevReedGrid[physRow];
            uint8_t curr = switches->grid[physRow];
            uint8_t placed = ~prev & curr;
            if (placed) {
                for (int col = 0; col < 8; col++) {
                    if (placed & (1 << col)) {
                        printf("Captured piece placed at black zone (%d, %d)\n", physRow, col);
                        // Don't increment here - getNextCaptureSlot handles it
                    }
                }
            }
        }
        for (int physRow = 10; physRow < 12; physRow++) {
            uint8_t prev = prevReedGrid[physRow];
            uint8_t curr = switches->grid[physRow];
            uint8_t placed = ~prev & curr;
            if (placed) {
                for (int col = 0; col < 8; col++) {
                    if (placed & (1 << col)) {
                        printf("Captured piece placed at white zone (%d, %d)\n", physRow, col);
                        // Don't increment here - getNextCaptureSlot handles it
                    }
                }
            }
        }

        // Update previous state
        memcpy(prevReedGrid, switches->grid, 12);

        vTaskDelay(pdMS_TO_TICKS(100));  // 100ms scan interval
    }
}

// Task to handle AI response moves in physical mode
void aiResponseTask(void *pvParameter) {
    ChessBoard* boardPtr = (ChessBoard*)pvParameter;
    int debugCounter = 0;

    printf("aiResponseTask started\n");

    while (1) {
        // Periodic debug output
        if (++debugCounter >= 50) {  // Every 5 seconds (50 * 100ms)
            debugCounter = 0;
            printf("AI Task: mode=%d aiReq=%d turn=%d playerCol=%d\n",
                   currentMode, aiMoveRequested ? 1 : 0, currentTurn, playerColor);
        }

        // Check if AI should make a move
        if (currentMode == MODE_PHYSICAL && aiMoveRequested && currentTurn != playerColor) {
            aiMoveRequested = false;

            printf("AI calculating move...\n");

            // Use ChessAI to find best move
            ChessAI ai;
            boardPtr->setTurn(currentTurn);
            AIMove aiMove = ai.findBestMove(*boardPtr, currentTurn, 3);

            if (aiMove.fromRow < 0) {
                printf("AI has no valid moves - game over?\n");
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            printf("AI move: (%d,%d) -> (%d,%d)\n",
                   aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol);

            // Convert chess coords to physical coords (matching LED mapping)
            // physRow = 9 - chessCol, physCol = chessRow
            int physFromRow = 9 - aiMove.fromCol;
            int physFromCol = aiMove.fromRow;
            int physToRow = 9 - aiMove.toCol;
            int physToCol = aiMove.toRow;

            // Check for special moves BEFORE regular capture detection
            bool isCastling = isCastlingMove(*boardPtr, aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol);
            bool isEnPassant = isEnPassantMove(*boardPtr, aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol);

            // Check if this is a regular capture move
            ChessPiece* targetPiece = boardPtr->getPiece(aiMove.toRow, aiMove.toCol);
            bool isCapture = (targetPiece != nullptr);

            MoveVerifyResult result = VERIFY_SUCCESS;

            if (isCastling) {
                // CASTLING: Move king to temp square, then rook, then king to final
                printf("AI: Executing castling move\n");
                bool kingSide = (aiMove.toCol > aiMove.fromCol);
                int rookSrcCol = kingSide ? 7 : 0;
                int rookDestCol = kingSide ? (aiMove.toCol - 1) : (aiMove.toCol + 1);

                // Convert rook positions to physical coords
                int physRookSrcRow = 9 - rookSrcCol;
                int physRookDestRow = 9 - rookDestCol;

                // Use temp square one column toward center from king (in physical coords)
                int tempRow = physFromRow;
                int tempCol = physFromCol + 1;  // Move toward center in physical space

                // Step 1: Move king to temp square
                printf("AI Castle Step 1: King to temp (%d,%d)\n", tempRow, tempCol);
                //plan_move(physFromRow, physFromCol, tempRow, tempCol, true);
                if (!wait_for_movement_complete(30000)) {
                    printf("AI castling step 1 timed out\n");
                    continue;
                }

                // Step 2: Move rook to final position
                printf("AI Castle Step 2: Rook (%d,%d) to (%d,%d)\n", physRookSrcRow, physFromCol, physRookDestRow, physFromCol);
                //plan_move(physRookSrcRow, physFromCol, physRookDestRow, physFromCol, true);
                if (!wait_for_movement_complete(30000)) {
                    printf("AI castling step 2 timed out\n");
                    continue;
                }

                // Step 3: Move king from temp to final position
                printf("AI Castle Step 3: King temp to final (%d,%d)\n", physToRow, physToCol);
                //plan_move(tempRow, tempCol, physToRow, physToCol, true);
                if (!wait_for_movement_complete(30000)) {
                    printf("AI castling step 3 timed out\n");
                    continue;
                }

                // Verify castling
                result = verify_castling_move(physFromRow, physFromCol, physToCol, physRookSrcRow, physRookDestRow);

            } else if (isEnPassant) {
                // EN PASSANT: Captured pawn is at different location than destination
                printf("AI: Executing en passant capture\n");
                Color aiColor = boardPtr->getPiece(aiMove.fromRow, aiMove.fromCol)->getColor();
                int capturedPawnRow = (aiColor == White) ? (aiMove.toRow + 1) : (aiMove.toRow - 1);
                // Captured pawn is at same column as destination, different row
                int physCapturedRow = 9 - aiMove.toCol;  // Same column as destination
                int physCapturedCol = capturedPawnRow;   // Row where captured pawn was
                Color opponentColor = (aiColor == White) ? Black : White;

                // Move captured pawn to capture zone
                auto [capZoneRow, capZoneCol] = getNextCaptureSlot(opponentColor);
                printf("AI: Moving en passant captured pawn from (%d,%d) to zone (%d,%d)\n",
                       physCapturedRow, physCapturedCol, capZoneRow, capZoneCol);
                //plan_move(physCapturedRow, physCapturedCol, capZoneRow, capZoneCol, false);
                if (!wait_for_movement_complete(30000)) {
                    printf("AI en passant capture timed out\n");
                    continue;
                }

                // Move attacking pawn to destination (en passant is single diagonal - always direct)
                //plan_move(physFromRow, physFromCol, physToRow, physToCol, true);
                if (!wait_for_movement_complete(30000)) {
                    printf("AI en passant move timed out\n");
                    continue;
                }

                result = verify_simple_move(physFromRow, physFromCol, physToRow, physToCol);

            } else if (isCapture) {
                // REGULAR CAPTURE
                auto [capZoneRow, capZoneCol] = getNextCaptureSlot(targetPiece->getColor());

                printf("AI: Moving captured %s piece to capture zone (%d, %d)\n",
                       targetPiece->getColor() == White ? "white" : "black",
                       capZoneRow, capZoneCol);

                // Move captured piece from destination to capture zone
                movePieceSmart(physToRow, physToCol, capZoneRow, capZoneCol);
                //plan_move(physToRow, physToCol, capZoneRow, capZoneCol, false);

                if (!wait_for_movement_complete(30000)) {
                    printf("AI capture move timed out\n");
                    continue;
                }

                // Verify capture zone placement
                if (!switches->isPopulated(capZoneRow, capZoneCol)) {
                    printf("Warning: Captured piece not detected in capture zone\n");
                }

                // Execute AI piece movement - use clear-path for any blockers
                // Note: Captured piece already moved to capture zone, but other pieces might still block
                {
                    ChessPiece* movingPiece = boardPtr->getPiece(aiMove.fromRow, aiMove.fromCol);
                    Type pieceType = movingPiece ? movingPiece->getType() : Pawn;
                    PathType pathType = PathAnalyzer::analyzeMovePath(aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol, *boardPtr, pieceType, false);
                    bool isDirect = (pathType == DIRECT_HORIZONTAL || pathType == DIRECT_VERTICAL ||
                                    pathType == DIAGONAL_DIRECT || pathType == DIRECT_L_PATH);
                    printf("AI: Capture move, path %s (%s)\n", isDirect ? "DIRECT" : "INDIRECT", PathAnalyzer::pathTypeToString(pathType).c_str());

                    // if (isDirect) {
                    //     plan_move(physFromRow, physFromCol, physToRow, physToCol, true);
                    // } else {
                        movePieceSmart(physFromRow, physFromCol, physToRow, physToCol);
                    // }
                }

                if (!wait_for_movement_complete(30000)) {
                    printf("AI move timed out\n");
                    continue;
                }

                result = verify_simple_move(physFromRow, physFromCol, physToRow, physToCol);

            } else {
                // REGULAR MOVE (no capture)
                // Check if direct path is clear using PathAnalyzer
                ChessPiece* movingPiece = boardPtr->getPiece(aiMove.fromRow, aiMove.fromCol);
                Type pieceType = movingPiece ? movingPiece->getType() : Pawn;
                PathType pathType = PathAnalyzer::analyzeMovePath(aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol, *boardPtr, pieceType, false);
                bool isDirect = (pathType == DIRECT_HORIZONTAL || pathType == DIRECT_VERTICAL ||
                                pathType == DIAGONAL_DIRECT || pathType == DIRECT_L_PATH);
                printf("AI: Regular move, path %s (%s)\n", isDirect ? "DIRECT" : "INDIRECT", PathAnalyzer::pathTypeToString(pathType).c_str());

                // if (isDirect) {
                //     plan_move(physFromRow, physFromCol, physToRow, physToCol, true);
                // } else {
                    movePieceSmart(physFromRow, physFromCol, physToRow, physToCol);
                //}

                if (!wait_for_movement_complete(30000)) {
                    printf("AI move timed out\n");
                    continue;
                }

                result = verify_simple_move(physFromRow, physFromCol, physToRow, physToCol);
            }

            while (result != VERIFY_SUCCESS) {
                ESP_LOGE("MOVE", "AI verification failed: %d", result);
                if (result == VERIFY_SOURCE_NOT_EMPTY) {
                    printf("ERROR: Piece not picked up from (%d,%d) - please remove it manually\n", physFromRow, physFromCol);
                } else if (result == VERIFY_DEST_NOT_OCCUPIED) {
                    printf("ERROR: Piece not placed at (%d,%d) - please place it manually\n", physToRow, physToCol);
                } else {
                    printf("ERROR: AI move verification failed (code %d) - manual intervention required\n", result);
                }
                printf("Waiting for board correction... (checking every 2 seconds)\n");

                vTaskDelay(pdMS_TO_TICKS(2000));
                result = verify_simple_move(physFromRow, physFromCol, physToRow, physToCol);
            }
            printf("AI move verified successfully\n");

            // Rest motors after AI move is complete
            rest_motors();

            // Update board state
            boardPtr->setTurn(currentTurn);
            boardPtr->movePiece(aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol);

            // Re-sync board_state for pathfinding after AI move
            setupMoveTracking(boardPtr);

            // Send updated board state to UI after AI move
            {
                uint8_t aiTxBuffer[33];
                vTaskDelay(pdMS_TO_TICKS(50));
                i2c_reset_tx_fifo(I2C_SLAVE_PORT);
                vTaskDelay(pdMS_TO_TICKS(50));
                if (playerColor == Black) {
                    serializeBoardStateFlipped(*boardPtr, aiTxBuffer);
                } else {
                    serializeBoardState(*boardPtr, aiTxBuffer);
                }
                i2c_slave_write_buffer(I2C_SLAVE_PORT, aiTxBuffer, sizeof(aiTxBuffer), pdMS_TO_TICKS(1000));
                printf("Sent AI move board state to UI (physical mode)\n");
            }

            // Update ambient lighting after AI move
            if (led != nullptr && firstMoveMade) {
                led->showAmbientWhite(*boardPtr);
            }

            // Check for game over (checkmate or stalemate)
            Color opponent = (currentTurn == White) ? Black : White;
            bool oppInCheck = boardPtr->isKingInCheck(opponent);
            bool oppCanMove = boardPtr->hasValidMoves(opponent);

            if (!oppCanMove) {
                uint8_t gameOverBuffer[33];
                if (oppInCheck) {
                    // Checkmate - use 0x2F
                    printf("Checkmate! %s wins!\n", (currentTurn == White) ? "White" : "Black");
                    if (playerColor == Black) {
                        serializeBoardStateFlippedGameOver(*boardPtr, gameOverBuffer);
                    } else {
                        serializeBoardStateGameOver(*boardPtr, gameOverBuffer);
                    }
                } else {
                    // Stalemate - use 0xEE
                    printf("Stalemate! Game is a draw.\n");
                    if (playerColor == Black) {
                        serializeBoardStateFlippedStalemate(*boardPtr, gameOverBuffer);
                    } else {
                        serializeBoardStateStalemate(*boardPtr, gameOverBuffer);
                    }
                }
                i2c_slave_write_buffer(I2C_SLAVE_PORT, gameOverBuffer, 33, pdMS_TO_TICKS(100));
            }

            // Flip turn back to human
            currentTurn = (currentTurn == White) ? Black : White;
            printf("Human's turn (%s)\n", (currentTurn == White) ? "White" : "Black");

            // Re-sync reed grid after AI move
            reedGridInitialized = false;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

extern "C" {
    void app_main(void);
}

void app_main(void) {
    
    // start_ota_server();

    ESP_LOGI(TAG, "Starting ESP32 Chess Game with I2C UI Integration");

    led = new leds();
    led->init();

    switches = new reed();
    switches->init();
    switches->start_scan_task();

    //speaker = new audio();
    //speaker->init();
    //play_error_tone();

    gpio_output_init(STEP1_PIN);
    gpio_output_init(STEP2_PIN);
    gpio_output_init(DIR1_PIN);
    gpio_output_init(MAGNET_PIN);
    gpio_output_init(DIR2_PIN);
    gpio_output_init(SLEEP_PIN);
    gpio_output_init(EN_PIN);
    gpio_output_init(HFS_PIN);
    gpio_output_init(MODE0_PIN);
    gpio_output_init(MODE1_PIN);
    gpio_output_init(MODE2_PIN);

    gpio_input_init(LIMIT_Y_PIN, GPIO_INTR_NEGEDGE);
    gpio_input_init(LIMIT_X_PIN, GPIO_INTR_NEGEDGE);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(LIMIT_Y_PIN, limit_switch_isr_handler, (void*) LIMIT_Y_PIN);
    gpio_isr_handler_add(LIMIT_X_PIN, limit_switch_isr_handler, (void*) LIMIT_X_PIN);

    gpio_set_level(SLEEP_PIN, 1);
    gpio_set_level(EN_PIN, 1);
    gpio_set_level(HFS_PIN, 1);
    gpio_set_level(MODE0_PIN, 0);
    gpio_set_level(MODE1_PIN, 0);
    gpio_set_level(MODE2_PIN, 1);

    setupMotion();
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI("INIT", "Startup, beginning homing sequence.");

    int homeOK = home_gantry();
    if (homeOK == -1) {
        ESP_LOGI("INIT", "Homing failed. Halting.");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    else{
        ESP_LOGI("INIT", "Homing sequence complete.");
    }

    if (homeOK != -1) {
        xTaskCreate(moveDispatchTask, "MoveDispatch", 8192, NULL, 10, NULL);
    }
    
    // ------------movement tests---------------
    // plan_move(0, 0, 2, 0, true);
    // // 100 move loop
    // for (int i = 0; i < 34; i++) {
    //     plan_move(2, 0, 9, 0, true);
    //     plan_move(9, 0, 9, 7, true);
    //     plan_move(9, 7, 2, 0, true);
    //     while(!gantry.position_reached || !move_queue_is_empty()) {
    //         vTaskDelay(pdMS_TO_TICKS(100));
    //     }
    //     ESP_LOGI("TEST", "Completed iteration %d of movement test", i+1);
    // }
    
    //test fix position
    // while (!gantry.position_reached || !move_queue_is_empty()) {
    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }
    // correct_movement(2, 0);


    ESP_LOGI("INIT", "Gantry motion and position status: active=%d reached=%d", gantry.motion_active ? 1 : 0, gantry.position_reached ? 1 : 0);


    // I2C slave init
    i2c_slave_init();
    ESP_LOGI("I2C", "I2C slave initialized at address 0x%02X", I2C_SLAVE_ADDRESS);

    // Initialize chess board
    ChessBoard board(8, 8);
    setupStandardBoard(board);
    ESP_LOGI("CHESS", "Chess board initialized");

    // Start physical pickup detection task
    xTaskCreate(piecePickupDetectionTask, "PiecePickup", 8192, &board, 5, NULL);
    ESP_LOGI("TASK", "Physical pickup detection task started");

    // Start AI response task
    xTaskCreate(aiResponseTask, "AIResponse", 16384, &board, 4, NULL);
    ESP_LOGI("TASK", "AI response task started");

    uint8_t rx_buffer[I2C_RX_BUF_LEN];
    uint8_t tx_buffer[33];  // For responses
    static uint8_t i2c_starter[3] = {0x1A, 0x2B, 0x3C};

    // Piece encoding reference:
    // 0=empty, 1-6=black (pawn,bishop,knight,rook,queen,king), 7-C=white

    while (1) {
        int bytes_read = i2c_slave_read_buffer(I2C_SLAVE_PORT, rx_buffer, sizeof(rx_buffer), pdMS_TO_TICKS(10));

        if (bytes_read > 0) {
            // Print received bytes for debugging
            printf("RX: ");
            for (int i = 0; i < bytes_read; i++) {
                printf("%02X ", rx_buffer[i]);
            }
            printf("\n");

            // Handle A1 B2 C3 handshake
            if (bytes_read >= 3 && rx_buffer[0] == 0xA1 && rx_buffer[1] == 0xB2 && rx_buffer[2] == 0xC3) {
                printf("Handshake received - responding with 1A 2B 3C\n");
                i2c_slave_write_buffer(I2C_SLAVE_PORT, i2c_starter, sizeof(i2c_starter), pdMS_TO_TICKS(100));
            }
            // Handle 3D - Game mode setup
            else if (rx_buffer[0] == 0x3D && bytes_read >= 2) {
                uint8_t mode = (rx_buffer[1] >> 4) & 0x0F;
                uint8_t color = rx_buffer[1] & 0x0F;
                currentMode = (mode == 1) ? MODE_PHYSICAL : MODE_UI;
                playerColor = (color == 1) ? Black : White;
                currentTurn = White;  // White always starts
                selectedRow = selectedCol = -1;

                printf("Game mode: %s, Player color: %s\n",
                       currentMode == MODE_PHYSICAL ? "Physical" : "UI",
                       playerColor == White ? "White" : "Black");

                // Reset board to starting position
                board = ChessBoard(8, 8);
                setupStandardBoard(board);
                setupMoveTracking(&board);

                // Reset capture zone counters
                capturedWhiteCount = 0;
                capturedBlackCount = 0;

                // Setup LED display based on mode
                if (currentMode == MODE_UI) {
                    // UI mode: Show WHITE ambient immediately (no setup phase needed)
                    showReedStateMode = false;
                    firstMoveMade = true;  // Skip setup phase for UI mode
                    boardSetupComplete = true;
                    pieceHeld = false;
                    if (led != nullptr) {
                        led->showAmbientWhite(board);
                    }
                    printf("UI mode: Showing ambient white lighting.\n");
                } else {
                    // Physical mode: Enable real-time reed LED display
                    if (playerColor == White) {
                        showReedStateMode = true;
                        firstMoveMade = false;
                        boardSetupComplete = false;  // Reset - wait for all pieces to be placed
                        pieceHeld = false;
                        printf("Board ready - place all pieces. LEDs will show RED/GREEN for setup.\n");
                    } else {
                        showReedStateMode = false;
                        firstMoveMade = false;
                        boardSetupComplete = false;  // Reset - wait for all pieces to be placed
                        pieceHeld = false;
                        printf("Board ready - place all pieces. AI will make first move after setup.\n");
                    }
                }

                // When player is black, AI (white) makes the first move
                if (playerColor == Black) {
                    if (currentMode == MODE_UI) {
                        // UI mode: AI makes first move immediately
                        printf("AI (White) making first move...\n");
                        ChessAI ai;
                        board.setTurn(White);
                        AIMove aiMove = ai.findBestMove(board, White, 3);
                        if (aiMove.fromRow >= 0) {
                            board.movePiece(aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol);
                            printf("AI opening move: (%d,%d) -> (%d,%d)\n",
                                   aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol);
                            currentTurn = Black;  // Now it's human's (black's) turn

                            // Show WHITE ambient under all pieces after AI's first move
                            if (led != nullptr) {
                                led->showAmbientWhite(board);
                            }
                        }
                    } else {
                        // Physical mode: Signal AI task to make first move
                        printf("Physical mode: Signaling AI to make first move\n");
                        aiMoveRequested = true;
                    }

                    // Send flipped board state (black's perspective)
                    i2c_reset_tx_fifo(I2C_SLAVE_PORT);
                    vTaskDelay(pdMS_TO_TICKS(5));
                    memset(tx_buffer, 0, sizeof(tx_buffer));
                    serializeBoardStateFlipped(board, tx_buffer);
                    i2c_slave_write_buffer(I2C_SLAVE_PORT, tx_buffer, 33, pdMS_TO_TICKS(100));
                    printf("Sent flipped board state for black player\n");
                }
            }
            // Handle AA - Piece selection (UI MODE ONLY)
            else if (rx_buffer[0] == 0xAA && bytes_read >= 2 && currentMode == MODE_UI) {
                int row = (rx_buffer[1] >> 4) & 0x0F;
                int col = rx_buffer[1] & 0x0F;

                // For black player, UI sends flipped coords - convert back to actual board coords
                if (playerColor == Black) {
                    row = 7 - row;
                    col = 7 - col;
                }

                ChessPiece* piece = board.getPiece(row, col);
                printf("Selection: row=%d, col=%d, piece=%s\n", row, col,
                       piece ? (piece->getColor() == White ? "White" : "Black") : "empty");

                // Check if this is a valid piece to select (player's own piece)
                bool validSelection = (piece != nullptr && piece->getColor() == currentTurn);

                if (validSelection) {
                    selectedRow = row;
                    selectedCol = col;

                    // Send possible moves (0xDD + 32 bytes)
                    i2c_reset_tx_fifo(I2C_SLAVE_PORT);
                    vTaskDelay(pdMS_TO_TICKS(5));
                    memset(tx_buffer, 0, sizeof(tx_buffer));
                    if (playerColor == Black) {
                        serializePossibleMovesFlipped(board, row, col, tx_buffer);
                    } else {
                        serializePossibleMoves(board, row, col, tx_buffer);
                    }
                    i2c_slave_write_buffer(I2C_SLAVE_PORT, tx_buffer, 33, pdMS_TO_TICKS(100));

                    auto moves = board.getPossibleMoves(row, col);
                    printf("Possible moves: %d (sent with 0xDD header): ", (int)moves.size());
                    for (int i = 0; i < 33; i++) {
                        printf("%02X", tx_buffer[i]);
                    }
                    printf("\n");

                    // Light up LEDs: BLUE on selected piece, GREEN on possible moves
                    if (led != nullptr) {
                        led->showPiecePickup(row, col, moves);
                    }
                } else {
                    // Clicked on empty square or opponent's piece - deselect
                    printf("Deselecting - clicked on %s\n", piece ? "opponent's piece" : "empty square");
                    selectedRow = selectedCol = -1;

                    // Restore ambient white LEDs
                    if (led != nullptr) {
                        led->clearPossibleMoves();
                        if (firstMoveMade) {
                            led->showAmbientWhiteFromReed(switches->grid);
                        }
                    }

                    // Still send empty possible moves response
                    i2c_reset_tx_fifo(I2C_SLAVE_PORT);
                    vTaskDelay(pdMS_TO_TICKS(5));
                    memset(tx_buffer, 0, sizeof(tx_buffer));
                    tx_buffer[0] = 0xDD;  // Header only, no moves
                    i2c_slave_write_buffer(I2C_SLAVE_PORT, tx_buffer, 33, pdMS_TO_TICKS(100));
                }
            }
            // Handle CC - Promotion piece selection response from UI
            else if (rx_buffer[0] == 0xCC && bytes_read >= 2) {
                uint8_t pieceCode = rx_buffer[1];
                Type selectedType = Queen; // default

                switch(pieceCode) {
                    case 2: selectedType = Rook; break;
                    case 3: selectedType = Knight; break;
                    case 4: selectedType = Bishop; break;
                    case 5: selectedType = Queen; break;
                    default:
                        printf("Unknown promotion piece code: %d, defaulting to Queen\n", pieceCode);
                        break;
                }

                if (promotionPending) {
                    promotionChoice = selectedType;
                    promotionPending = false;
                    printf("Promotion choice received: %d -> Type %d\n", pieceCode, (int)selectedType);
                } else {
                    printf("Received promotion choice but no promotion pending\n");
                }
            }
            // Handle FF - Move execution (UI MODE ONLY)
            else if (rx_buffer[0] == 0xFF && bytes_read >= 2 && currentMode == MODE_UI) {
                int toRow = (rx_buffer[1] >> 4) & 0x0F;
                int toCol = rx_buffer[1] & 0x0F;

                // For black player, UI sends flipped coords - convert back to actual board coords
                if (playerColor == Black) {
                    toRow = 7 - toRow;
                    toCol = 7 - toCol;
                }

                printf("Move request: (%d,%d) -> (%d,%d)\n", selectedRow, selectedCol, toRow, toCol);

                if (selectedRow >= 0 && selectedCol >= 0) {
                    // Detect special moves BEFORE capture check
                    bool isCastlingMoveFlag = isCastlingMove(board, selectedRow, selectedCol, toRow, toCol);
                    bool isEnPassantMoveFlag = isEnPassantMove(board, selectedRow, selectedCol, toRow, toCol);

                    // Check for pawn promotion BEFORE physical movement
                    Type promotionType = Queen;  // Default to Queen
                    ChessPiece* movingPiece = board.getPiece(selectedRow, selectedCol);
                    if (movingPiece && movingPiece->getType() == Pawn) {
                        bool isPromotion = (movingPiece->getColor() == White && toRow == 0) ||
                                          (movingPiece->getColor() == Black && toRow == 7);
                        if (isPromotion && currentMode == MODE_UI) {
                            printf("Pawn promotion detected! Requesting piece choice from UI...\n");
                            promotionType = waitForPromotionChoice(board, 30000);
                            printf("Promotion piece selected: %d\n", (int)promotionType);
                        }
                    }

                    // Check for regular capture BEFORE moving piece
                    ChessPiece* capturedPiece = board.getPiece(toRow, toCol);
                    bool isCapture = (capturedPiece != nullptr);
                    Color capturedColor = isCapture ? capturedPiece->getColor() : White;

                    // Store info needed for physical movements BEFORE updating board
                    ChessPiece* movingPiecePtr = board.getPiece(selectedRow, selectedCol);
                    Color movingColor = movingPiecePtr ? movingPiecePtr->getColor() : currentTurn;
                    Type movingPieceType = movingPiecePtr ? movingPiecePtr->getType() : Pawn;

                    // Update the logical board state FIRST (before physical movements)
                    board.setTurn(currentTurn);
                    if (!board.movePiece(selectedRow, selectedCol, toRow, toCol, false, promotionType)) {
                        printf("Move failed - invalid move\n");
                        selectedRow = selectedCol = -1;
                        if (led != nullptr) {
                            led->clearPossibleMoves();
                        }
                        continue;
                    }

                    printf("Move successful! %s moved from (%d,%d) to (%d,%d)\n",
                           currentTurn == White ? "White" : "Black",
                           selectedRow, selectedCol, toRow, toCol);

                    // Disable reed state LED display after first move
                    if (!firstMoveMade) {
                        firstMoveMade = true;
                        showReedStateMode = false;
                        if (led != nullptr) {
                            led->clearPossibleMoves();
                        }
                        printf("First move made - reed state display disabled\n");
                    }

                    currentTurn = (currentTurn == White) ? Black : White;
                    printf("Turn changed to: %s\n", currentTurn == White ? "White" : "Black");

                    // Send updated board state to UI IMMEDIATELY (before physical movements)
                    {
                        vTaskDelay(pdMS_TO_TICKS(50));
                        i2c_reset_tx_fifo(I2C_SLAVE_PORT);
                        vTaskDelay(pdMS_TO_TICKS(50));
                        if (playerColor == Black) {
                            serializeBoardStateFlipped(board, tx_buffer);
                        } else {
                            serializeBoardState(board, tx_buffer);
                        }
                        i2c_slave_write_buffer(I2C_SLAVE_PORT, tx_buffer, sizeof(tx_buffer), pdMS_TO_TICKS(1000));
                        printf("Sent player move board state to UI\n");
                    }

                    // Show WHITE ambient under all pieces
                    if (led != nullptr) {
                        led->showAmbientWhite(board);
                    }

                    // Physical movement and capture handling
                    {
                        // Physical coords: matching LED mapping
                        // physRow = 9 - chessCol, physCol = chessRow
                        int physFromRow = 9 - selectedCol;
                        int physFromCol = selectedRow;
                        int physToRow = 9 - toCol;
                        int physToCol = toRow;

                        MoveVerifyResult result = VERIFY_SUCCESS;

                        if (isCastlingMoveFlag) {
                            // CASTLING: Move king to temp square, then rook, then king to final
                            printf("UI: Executing castling move\n");
                            bool kingSide = (toCol > selectedCol);
                            int rookSrcCol = kingSide ? 7 : 0;
                            int rookDestCol = kingSide ? (toCol - 1) : (toCol + 1);

                            // Convert rook positions to physical coords
                            int physRookSrcRow = 9 - rookSrcCol;
                            int physRookDestRow = 9 - rookDestCol;

                            // Use temp square one column toward center from king (in physical coords)
                            int tempRow = physFromRow;
                            int tempCol = physFromCol + 1;  // Move toward center in physical space

                            // Step 1: Move king to temp square
                            printf("Castle Step 1: King to temp (%d,%d)\n", tempRow, tempCol);
                            //plan_move(physFromRow, physFromCol, tempRow, tempCol, true);
                            if (!wait_for_movement_complete(30000)) {
                                ESP_LOGE("MOVE", "Castling step 1 timeout");
                                continue;
                            }

                            // Step 2: Move rook to final position
                            printf("Castle Step 2: Rook (%d,%d) to (%d,%d)\n", physRookSrcRow, physFromCol, physRookDestRow, physFromCol);
                            //plan_move(physRookSrcRow, physFromCol, physRookDestRow, physFromCol, true);
                            if (!wait_for_movement_complete(30000)) {
                                ESP_LOGE("MOVE", "Castling step 2 timeout");
                                continue;
                            }

                            // Step 3: Move king from temp to final position
                            printf("Castle Step 3: King temp to final (%d,%d)\n", physToRow, physToCol);
                            //plan_move(tempRow, tempCol, physToRow, physToCol, true);
                            if (!wait_for_movement_complete(30000)) {
                                ESP_LOGE("MOVE", "Castling step 3 timeout");
                                continue;
                            }

                            // Verify castling
                            result = verify_castling_move(physFromRow, physFromCol, physToCol, physRookSrcRow, physRookDestRow);

                        } else if (isEnPassantMoveFlag) {
                            // EN PASSANT: Captured pawn is at different location than destination
                            printf("UI: Executing en passant capture\n");
                            // Use stored movingColor since board state already updated
                            int capturedPawnRow = (movingColor == White) ? (toRow + 1) : (toRow - 1);
                            // Captured pawn is at same column as destination, different row
                            int physCapturedRow = 9 - toCol;      // Same column as destination
                            int physCapturedCol = capturedPawnRow; // Row where captured pawn was
                            Color opponentColor = (movingColor == White) ? Black : White;

                            // Move captured pawn to capture zone
                            auto [capZoneRow, capZoneCol] = getNextCaptureSlot(opponentColor);
                            printf("UI: Moving en passant captured pawn from (%d,%d) to zone (%d,%d)\n",
                                   physCapturedRow, physCapturedCol, capZoneRow, capZoneCol);
                            //plan_move(physCapturedRow, physCapturedCol, capZoneRow, capZoneCol, false);
                            if (!wait_for_movement_complete(30000)) {
                                ESP_LOGE("MOVE", "En passant capture timeout");
                                continue;
                            }

                            // Move attacking pawn to destination (en passant is single diagonal - always direct)
                            //plan_move(physFromRow, physFromCol, physToRow, physToCol, true);
                            if (!wait_for_movement_complete(30000)) {
                                ESP_LOGE("MOVE", "En passant move timeout");
                                continue;
                            }

                            result = verify_simple_move(physFromRow, physFromCol, physToRow, physToCol);

                        } else if (isCapture) {
                            // REGULAR CAPTURE - use stored capturedColor since board already updated
                            auto [capZoneRow, capZoneCol] = getNextCaptureSlot(capturedColor);

                            printf("UI: Moving captured %s piece to capture zone (%d, %d)\n",
                                   capturedColor == White ? "white" : "black",
                                   capZoneRow, capZoneCol);

                            movePieceSmart(physToRow, physToCol, capZoneRow, capZoneCol);       
                            //plan_move(physToRow, physToCol, capZoneRow, capZoneCol, false);

                            if (!wait_for_movement_complete(30000)) {
                                ESP_LOGE("MOVE", "Capture movement timeout");
                                printf("ERROR: Capture movement timeout\n");
                                continue;
                            }

                            // Now move the player's piece - use clear-path for any blockers
                            {
                                // Use stored movingPieceType since board already updated
                                PathType pathType = PathAnalyzer::analyzeMovePath(selectedRow, selectedCol, toRow, toCol, board, movingPieceType, false);
                                bool isDirect = (pathType == DIRECT_HORIZONTAL || pathType == DIRECT_VERTICAL ||
                                                pathType == DIAGONAL_DIRECT || pathType == DIRECT_L_PATH);
                                printf("Player: Capture move, path %s (%s)\n", isDirect ? "DIRECT" : "INDIRECT", PathAnalyzer::pathTypeToString(pathType).c_str());

                                // if (isDirect) {
                                //     plan_move(physFromRow, physFromCol, physToRow, physToCol, true);
                                // } else {
                                    movePieceSmart(physFromRow, physFromCol, physToRow, physToCol);
                                //}
                            }

                            if (!wait_for_movement_complete(30000)) {
                                ESP_LOGE("MOVE", "Movement timeout");
                                continue;
                            }

                            result = verify_simple_move(physFromRow, physFromCol, physToRow, physToCol);

                        } else {
                            // REGULAR MOVE (no capture)
                            // Use stored movingPieceType since board already updated
                            PathType pathType = PathAnalyzer::analyzeMovePath(selectedRow, selectedCol, toRow, toCol, board, movingPieceType, false);
                            bool isDirect = (pathType == DIRECT_HORIZONTAL || pathType == DIRECT_VERTICAL ||
                                            pathType == DIAGONAL_DIRECT || pathType == DIRECT_L_PATH);
                            printf("Player: Regular move, path %s (%s)\n", isDirect ? "DIRECT" : "INDIRECT", PathAnalyzer::pathTypeToString(pathType).c_str());

                            // if (isDirect) {
                            //     plan_move(physFromRow, physFromCol, physToRow, physToCol, true);
                            // } else {
                                movePieceSmart(physFromRow, physFromCol, physToRow, physToCol);
                            //}

                            if (!wait_for_movement_complete(30000)) {
                                ESP_LOGE("MOVE", "Movement timeout - manual intervention required");
                                printf("ERROR: Movement timeout\n");
                                continue;
                            }

                            result = verify_simple_move(physFromRow, physFromCol, physToRow, physToCol);
                        }

                        while (result != VERIFY_SUCCESS) {
                            ESP_LOGE("MOVE", "Verification failed: %d", result);
                            if (result == VERIFY_SOURCE_NOT_EMPTY) {
                                printf("ERROR: Piece not picked up from (%d,%d) - please remove it manually\n", physFromRow, physFromCol);
                            } else if (result == VERIFY_DEST_NOT_OCCUPIED) {
                                printf("ERROR: Piece not placed at (%d,%d) - please place it manually\n", physToRow, physToCol);
                            } else {
                                printf("ERROR: Move verification failed (code %d) - manual intervention required\n", result);
                            }
                            printf("Waiting for board correction... (checking every 2 seconds)\n");

                            // Wait and retry verification
                            vTaskDelay(pdMS_TO_TICKS(2000));
                            result = verify_simple_move(physFromRow, physFromCol, physToRow, physToCol);
                        }
                        printf("Move verified successfully via reed switches\n");
                    }

                    // In UI mode, if it's AI's turn, make AI move
                    if (currentMode == MODE_UI && currentTurn != playerColor) {
                            printf("AI thinking (minimax depth 2)...\n");
                            ChessAI ai;
                            AIMove aiMove = ai.findBestMove(board, currentTurn, 2);
                            if (aiMove.fromRow >= 0) {
                                printf("AI move: (%d,%d) -> (%d,%d), score=%.2f\n",
                                       aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol, aiMove.score);

                                // Convert chess coords to physical coords (matching LED mapping)
                                int physFromRow = 9 - aiMove.fromCol;
                                int physFromCol = aiMove.fromRow;
                                int physToRow = 9 - aiMove.toCol;
                                int physToCol = aiMove.toRow;

                                // Detect special moves and store info BEFORE modifying board
                                bool isCastling = isCastlingMove(board, aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol);
                                bool isEnPassant = isEnPassantMove(board, aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol);
                                ChessPiece* targetPiece = board.getPiece(aiMove.toRow, aiMove.toCol);
                                bool isCapture = (targetPiece != nullptr);
                                Color aiCapturedColor = isCapture ? targetPiece->getColor() : White;
                                ChessPiece* movingPiece = board.getPiece(aiMove.fromRow, aiMove.fromCol);
                                Color aiColor = movingPiece ? movingPiece->getColor() : currentTurn;
                                Type aiPieceType = movingPiece ? movingPiece->getType() : Pawn;

                                // Update logical board FIRST
                                board.setTurn(currentTurn);
                                board.movePiece(aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol);
                                printf("AI moved: (%d,%d) -> (%d,%d)\n",
                                       aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol);

                                currentTurn = (currentTurn == White) ? Black : White;
                                printf("Turn changed to: %s\n", currentTurn == White ? "White" : "Black");

                                // Send AI move board state to UI IMMEDIATELY
                                {
                                    vTaskDelay(pdMS_TO_TICKS(50));
                                    i2c_reset_tx_fifo(I2C_SLAVE_PORT);
                                    vTaskDelay(pdMS_TO_TICKS(50));
                                    if (playerColor == Black) {
                                        serializeBoardStateFlipped(board, tx_buffer);
                                    } else {
                                        serializeBoardState(board, tx_buffer);
                                    }
                                    i2c_slave_write_buffer(I2C_SLAVE_PORT, tx_buffer, sizeof(tx_buffer), pdMS_TO_TICKS(1000));
                                    printf("Sent AI move board state to UI\n");
                                }

                                // Show WHITE ambient
                                if (led != nullptr) {
                                    led->showAmbientWhite(board);
                                }

                                MoveVerifyResult result = VERIFY_SUCCESS;

                                if (isCastling) {
                                    // CASTLING
                                    printf("UI AI: Executing castling move\n");
                                    bool kingSide = (aiMove.toCol > aiMove.fromCol);
                                    int rookSrcCol = kingSide ? 7 : 0;
                                    int rookDestCol = kingSide ? (aiMove.toCol - 1) : (aiMove.toCol + 1);
                                    int physRookSrcRow = 9 - rookSrcCol;
                                    int physRookDestRow = 9 - rookDestCol;
                                    int tempRow = physFromRow;
                                    int tempCol = physFromCol + 1;

                                    //plan_move(physFromRow, physFromCol, tempRow, tempCol, true);
                                    wait_for_movement_complete(30000);
                                    //plan_move(physRookSrcRow, physFromCol, physRookDestRow, physFromCol, true);
                                    wait_for_movement_complete(30000);
                                    //plan_move(tempRow, tempCol, physToRow, physToCol, true);
                                    wait_for_movement_complete(30000);
                                    result = verify_castling_move(physFromRow, physFromCol, physToCol, physRookSrcRow, physRookDestRow);

                                } else if (isEnPassant) {
                                    // EN PASSANT
                                    printf("UI AI: Executing en passant\n");
                                    int capturedPawnRow = (aiColor == White) ? (aiMove.toRow + 1) : (aiMove.toRow - 1);
                                    int physCapturedRow = 9 - aiMove.toCol;
                                    int physCapturedCol = capturedPawnRow;
                                    Color opponentColor = (aiColor == White) ? Black : White;
                                    auto [capZoneRow, capZoneCol] = getNextCaptureSlot(opponentColor);

                                    //plan_move(physCapturedRow, physCapturedCol, capZoneRow, capZoneCol, false);
                                    wait_for_movement_complete(30000);
                                    // En passant pawn move is single diagonal - always direct
                                    //plan_move(physFromRow, physFromCol, physToRow, physToCol, true);
                                    wait_for_movement_complete(30000);
                                    result = verify_simple_move(physFromRow, physFromCol, physToRow, physToCol);

                                } else if (isCapture) {
                                    // REGULAR CAPTURE - use stored aiCapturedColor
                                    auto [capZoneRow, capZoneCol] = getNextCaptureSlot(aiCapturedColor);
                                    printf("UI AI: Capture move, target to zone (%d,%d)\n", capZoneRow, capZoneCol);

                                    movePieceSmart(physToRow, physToCol, capZoneRow, capZoneCol);
                                    //plan_move(physToRow, physToCol, capZoneRow, capZoneCol, false);
                                    wait_for_movement_complete(30000);

                                    // Move capturing piece - use stored aiPieceType
                                    {
                                        PathType pathType = PathAnalyzer::analyzeMovePath(aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol, board, aiPieceType, false);
                                        bool isDirect = (pathType == DIRECT_HORIZONTAL || pathType == DIRECT_VERTICAL ||
                                                        pathType == DIAGONAL_DIRECT || pathType == DIRECT_L_PATH);
                                        printf("UI AI: Capture move, path %s (%s)\n", isDirect ? "DIRECT" : "INDIRECT", PathAnalyzer::pathTypeToString(pathType).c_str());

                                        // if (isDirect) {
                                        //     plan_move(physFromRow, physFromCol, physToRow, physToCol, true);
                                        // } else {
                                            movePieceSmart(physFromRow, physFromCol, physToRow, physToCol);
                                        //}
                                    }
                                    wait_for_movement_complete(30000);
                                    result = verify_simple_move(physFromRow, physFromCol, physToRow, physToCol);

                                } else {
                                    // REGULAR MOVE - use stored aiPieceType
                                    PathType pathType = PathAnalyzer::analyzeMovePath(aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol, board, aiPieceType, false);
                                    bool isDirect = (pathType == DIRECT_HORIZONTAL || pathType == DIRECT_VERTICAL ||
                                                    pathType == DIAGONAL_DIRECT || pathType == DIRECT_L_PATH);
                                    printf("UI AI: Regular move, path %s (%s)\n", isDirect ? "DIRECT" : "INDIRECT", PathAnalyzer::pathTypeToString(pathType).c_str());

                                    // if (isDirect) {
                                    //     plan_move(physFromRow, physFromCol, physToRow, physToCol, true);
                                    // } else {
                                    movePieceSmart(physFromRow, physFromCol, physToRow, physToCol);
                                    //}
                                    wait_for_movement_complete(30000);
                                    result = verify_simple_move(physFromRow, physFromCol, physToRow, physToCol);
                                }

                                while (result != VERIFY_SUCCESS) {
                                    ESP_LOGE("MOVE", "UI AI verification failed: %d", result);
                                    if (result == VERIFY_SOURCE_NOT_EMPTY) {
                                        printf("ERROR: Piece not picked up from (%d,%d) - please remove it manually\n", physFromRow, physFromCol);
                                    } else if (result == VERIFY_DEST_NOT_OCCUPIED) {
                                        printf("ERROR: Piece not placed at (%d,%d) - please place it manually\n", physToRow, physToCol);
                                    } else {
                                        printf("ERROR: UI AI move verification failed (code %d) - manual intervention required\n", result);
                                    }
                                    printf("Waiting for board correction... (checking every 2 seconds)\n");

                                    vTaskDelay(pdMS_TO_TICKS(2000));
                                    result = verify_simple_move(physFromRow, physFromCol, physToRow, physToCol);
                                }
                                printf("UI AI move verified successfully\n");

                                // Rest motors after AI move is complete
                                rest_motors();
                            } else {
                                printf("AI has no valid moves!\n");
                            }
                    }
                } else {
                    printf("Move failed - no piece selected\n");
                }

                selectedRow = selectedCol = -1;

                // Clear LED highlights after move attempt
                if (led != nullptr) {
                    led->clearPossibleMoves();
                }

                // Check for game-over conditions (checkmate or stalemate)
                bool gameOver = false;
                bool inCheck = isKingInCheck(board, currentTurn);
                bool canMove = hasValidMoves(board, currentTurn);
                //bool inCheck = false;
                //bool canMove = false;

                if (!canMove) {
                    gameOver = true;
                    if (inCheck) {
                        // Checkmate - current player (who is about to move) loses
                        Color winner = (currentTurn == White) ? Black : White;
                        printf("CHECKMATE! %s wins!\n", winner == White ? "White" : "Black");
                    } else {
                        // Stalemate - draw
                        printf("STALEMATE! Game is a draw.\n");
                    }
                }

                // Send updated board state (after AI move if applicable)
                vTaskDelay(pdMS_TO_TICKS(100));
                i2c_reset_tx_fifo(I2C_SLAVE_PORT);
                vTaskDelay(pdMS_TO_TICKS(100));

                // Use game-over preamble (0x2F for checkmate, 0xEE for stalemate)
                // Use flipped version for black player perspective
                if (gameOver) {
                    if (inCheck) {
                        // Checkmate - use 0x2F
                        if (playerColor == Black) {
                            serializeBoardStateFlippedGameOver(board, tx_buffer);
                        } else {
                            serializeBoardStateGameOver(board, tx_buffer);
                        }
                        printf("Sent CHECKMATE board state with 0x2F preamble (33 bytes): ");
                    } else {
                        // Stalemate - use 0xEE
                        if (playerColor == Black) {
                            serializeBoardStateFlippedStalemate(board, tx_buffer);
                        } else {
                            serializeBoardStateStalemate(board, tx_buffer);
                        }
                        printf("Sent STALEMATE board state with 0xEE preamble (33 bytes): ");
                    }
                } else {
                    if (playerColor == Black) {
                        serializeBoardStateFlipped(board, tx_buffer);
                    } else {
                        serializeBoardState(board, tx_buffer);
                    }
                    printf("Sent updated board state (33 bytes): ");
                }

                vTaskDelay(pdMS_TO_TICKS(100));
                i2c_slave_write_buffer(I2C_SLAVE_PORT, tx_buffer, sizeof(tx_buffer), pdMS_TO_TICKS(1000));
                for (int i = 0; i < 33; i++) {
                    printf("%02X", tx_buffer[i]);
                }
                printf("\n");

                // Print board to terminal for debugging
                string boardStr = board.displayExpandedBoard().str();
                printf("\n%s\n", boardStr.c_str());
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}