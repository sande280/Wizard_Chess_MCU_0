#include "ChessBoard.hh"
#include "ChessPiece.hh"
#include <vector>
#include <cstdlib>
#include <ctime>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace Student {

struct SimpleMove {
    int fromRow, fromCol, toRow, toCol;
    int value;
};

int getSimplePieceValue(Type type) {
    switch(type) {
        case Pawn: return 1;
        case Knight: return 3;
        case Bishop: return 3;
        case Rook: return 5;
        case Queen: return 9;
        case King: return 100;
        default: return 0;
    }
}

SimpleMove findBestSimpleMove(ChessBoard& board, Color aiColor) {
    SimpleMove bestMove = {-1, -1, -1, -1, -1000};
    std::vector<SimpleMove> validMoves;

    // Save current turn
    Color savedTurn = board.getTurn();
    board.setTurn(aiColor);

    // Find all valid moves
    for (int fromRow = 0; fromRow < 8; fromRow++) {
        for (int fromCol = 0; fromCol < 8; fromCol++) {
            ChessPiece* piece = board.getPiece(fromRow, fromCol);
            if (piece && piece->getColor() == aiColor) {

                // Get possible moves for this piece
                std::vector<std::pair<int, int>> possibleMoves = board.getPossibleMoves(fromRow, fromCol);

                for (const auto& move : possibleMoves) {
                    SimpleMove simpleMove;
                    simpleMove.fromRow = fromRow;
                    simpleMove.fromCol = fromCol;
                    simpleMove.toRow = move.first;
                    simpleMove.toCol = move.second;

                    // Calculate move value
                    simpleMove.value = 0;

                    // Check for capture
                    ChessPiece* target = board.getPiece(move.first, move.second);
                    if (target) {
                        simpleMove.value = getSimplePieceValue(target->getType()) * 10;
                    }

                    // Small random factor to avoid always playing the same moves
                    simpleMove.value += (rand() % 3);

                    // Prefer center control
                    if ((move.first == 3 || move.first == 4) && (move.second == 3 || move.second == 4)) {
                        simpleMove.value += 2;
                    }

                    validMoves.push_back(simpleMove);
                }

                // Yield periodically to prevent watchdog timeout
                vTaskDelay(1 / portTICK_PERIOD_MS);
            }
        }
    }

    // Restore turn
    board.setTurn(savedTurn);

    // Find best move
    if (!validMoves.empty()) {
        bestMove = validMoves[0];
        for (const auto& move : validMoves) {
            if (move.value > bestMove.value) {
                bestMove = move;
            }
        }
    }

    return bestMove;
}

}