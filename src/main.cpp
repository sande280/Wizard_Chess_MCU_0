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

#include "Chess.h"
#include "ChessBoard.hh"
#include "ChessPiece.hh"
#include "BoardStateTracker.hh"
#include "BoardStates.hh"
#include "PathAnalyzer.hh"
#include "ChessAI.hh"

#include "move_queue.h"
#include "motionPos.h"
#include <PinDefs.h>
#include "step_timer.h"

esp_timer_handle_t step_timer = nullptr;

using namespace Student;
using namespace std;

static const char *TAG = "ESP_CHESS";

#define UART_NUM UART_NUM_0
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)





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
}

#define I2C_SLAVE_SDA_IO        10
#define I2C_SLAVE_SCL_IO        11
#define I2C_SLAVE_PORT          I2C_NUM_0
#define I2C_SLAVE_ADDRESS       0x67
#define I2C_RX_BUF_LEN          256
#define I2C_TX_BUF_LEN          256

// Game state for I2C UI integration
enum GameMode { MODE_NONE, MODE_PHYSICAL, MODE_UI };
static GameMode currentMode = MODE_NONE;
static Color playerColor = White;  // Human player's color
static Color currentTurn = White;
static int selectedRow = -1, selectedCol = -1;

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
        },
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


bool moveToXY(float x_target_mm, float y_target_mm, float speed_mm_s, bool magnet_on) {
    
    
    gpio_set_level(MAGNET_PIN, magnet_on);

    if (speed_mm_s <= 0.0f) return false;


    float dx = x_target_mm - gantry.x;
    float dy = y_target_mm - gantry.y;
    float distance = sqrtf(dx*dx + dy*dy);
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
                ESP_LOGI("MOVE", "Dispatching queued move to (%.2f, %.2f) speed=%.1f magnet=%d", next.x, next.y, next.speed, next.magnet ? 1 : 0);
                moveToXY(next.x, next.y, next.speed, next.magnet);
            }
            else {
                ESP_LOGI("INIT", "Pop failed unexpectedly");
            }
        }

        if (gantry.position_reached) {
            ESP_LOGI("MOVE", "Idle: position reached");
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


extern "C" {
    void app_main(void);
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting ESP32 Chess Game with I2C UI Integration");
    /*
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

    // movement tests
    plan_move(0, 0, 5, 3, true);
    plan_move(5, 3, 6, 4, true);
    plan_move(6, 4, 6, 6, true);
    // plan_move(6, 6, 8, 7, true);
    plan_move(10, 7, 0, 0, false);
    ESP_LOGI("INIT", "Gantry motion and position status: active=%d reached=%d", gantry.motion_active ? 1 : 0, gantry.position_reached ? 1 : 0);

    if (homeOK != -1) {
        xTaskCreate(moveDispatchTask, "MoveDispatch", 8192, NULL, 10, NULL);
    }
    */
    // I2C slave init
    i2c_slave_init();
    ESP_LOGI("I2C", "I2C slave initialized at address 0x%02X", I2C_SLAVE_ADDRESS);

    // Initialize chess board
    ChessBoard board(8, 8);
    setupStandardBoard(board);
    ESP_LOGI("CHESS", "Chess board initialized");

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

                // Only send initial board state for black player (UI reads for black, not white)
                if (playerColor == Black) {
                    i2c_reset_tx_fifo(I2C_SLAVE_PORT);  // Clear any stale data
                    vTaskDelay(pdMS_TO_TICKS(5));  // Small delay after FIFO clear
                    memset(tx_buffer, 0, sizeof(tx_buffer));
                    serializeBoardState(board, tx_buffer);
                    i2c_slave_write_buffer(I2C_SLAVE_PORT, tx_buffer, 33, pdMS_TO_TICKS(100));
                    printf("Sent initial board state for black player\n");
                }
            }
            // Handle AA - Piece selection
            else if (rx_buffer[0] == 0xAA && bytes_read >= 2) {
                int row = (rx_buffer[1] >> 4) & 0x0F;
                int col = rx_buffer[1] & 0x0F;
                selectedRow = row;
                selectedCol = col;

                ChessPiece* piece = board.getPiece(row, col);
                printf("Selection: row=%d, col=%d, piece=%s\n", row, col,
                       piece ? (piece->getColor() == White ? "White" : "Black") : "empty");

                // Send possible moves (0xDD + 32 bytes)
                i2c_reset_tx_fifo(I2C_SLAVE_PORT);
                vTaskDelay(pdMS_TO_TICKS(5));
                memset(tx_buffer, 0, sizeof(tx_buffer));
                serializePossibleMoves(board, row, col, tx_buffer);
                i2c_slave_write_buffer(I2C_SLAVE_PORT, tx_buffer, 33, pdMS_TO_TICKS(100));

                auto moves = board.getPossibleMoves(row, col);
                printf("Possible moves: %d (sent with 0xDD header): ", (int)moves.size());
                for (int i = 0; i < 33; i++) {
                    printf("%02X", tx_buffer[i]);
                }
                printf("\n");
            }
            // Handle FF - Move execution
            else if (rx_buffer[0] == 0xFF && bytes_read >= 2) {
                int toRow = (rx_buffer[1] >> 4) & 0x0F;
                int toCol = rx_buffer[1] & 0x0F;
                printf("Move request: (%d,%d) -> (%d,%d)\n", selectedRow, selectedCol, toRow, toCol);

                bool moveSuccess = false;
                if (selectedRow >= 0 && selectedCol >= 0) {
                    board.setTurn(currentTurn);
                    if (board.movePiece(selectedRow, selectedCol, toRow, toCol)) {
                        printf("Move successful! %s moved from (%d,%d) to (%d,%d)\n",
                               currentTurn == White ? "White" : "Black",
                               selectedRow, selectedCol, toRow, toCol);
                        moveSuccess = true;

                        // PLACEHOLDER: Physical movement
                        // In physical mode, would trigger motor movement here
                        if (currentMode == MODE_PHYSICAL) {
                            printf("PLACEHOLDER: Motor would move piece from (%d,%d) to (%d,%d)\n",
                                   selectedRow, selectedCol, toRow, toCol);
                        }

                        currentTurn = (currentTurn == White) ? Black : White;
                        printf("Turn changed to: %s\n", currentTurn == White ? "White" : "Black");

                        // In UI mode, if it's AI's turn, make AI move
                        if (currentMode == MODE_UI && currentTurn != playerColor) {
                            printf("AI thinking (minimax depth 3)...\n");
                            ChessAI ai;
                            AIMove aiMove = ai.findBestMove(board, currentTurn, 3);
                            if (aiMove.fromRow >= 0) {
                                board.setTurn(currentTurn);
                                board.movePiece(aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol);
                                printf("AI moved: (%d,%d) -> (%d,%d), score=%.2f\n",
                                       aiMove.fromRow, aiMove.fromCol, aiMove.toRow, aiMove.toCol, aiMove.score);

                                // PLACEHOLDER: Physical movement for AI
                                if (currentMode == MODE_PHYSICAL) {
                                    printf("PLACEHOLDER: Motor would move AI piece\n");
                                }

                                currentTurn = (currentTurn == White) ? Black : White;
                                printf("Turn changed to: %s\n", currentTurn == White ? "White" : "Black");
                            } else {
                                printf("AI has no valid moves!\n");
                            }
                        }
                    } else {
                        printf("Move failed - invalid move\n");
                    }
                } else {
                    printf("Move failed - no piece selected\n");
                }

                selectedRow = selectedCol = -1;

                // Send updated board state (after AI move if applicable)
                //i2c_reset_tx_fifo(I2C_SLAVE_PORT);  // Clear any stale data
                //vTaskDelay(pdMS_TO_TICKS(50));  // Small delay after FIFO clear
                //memset(tx_buffer, 0, sizeof(tx_buffer));
                vTaskDelay(pdMS_TO_TICKS(100));
                i2c_reset_tx_fifo(I2C_SLAVE_PORT);
                vTaskDelay(pdMS_TO_TICKS(100));


                serializeBoardState(board, tx_buffer);
                //serializeBoardState(board, tx_buffer);
                vTaskDelay(pdMS_TO_TICKS(100));
                //vTaskDelay(pdMS_TO_TICKS(500));
                //tx_buffer = {AA, 40, 25, 6234111111110030000000000000000070000000000077770777A98BC89A;
                i2c_slave_write_buffer(I2C_SLAVE_PORT, tx_buffer, sizeof(tx_buffer), pdMS_TO_TICKS(1000));
                printf("Sent updated board state (33 bytes): ");
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