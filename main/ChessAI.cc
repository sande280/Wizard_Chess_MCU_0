#include "ChessAI.hh"
#include <algorithm>
#include <random>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace Student {

int ChessAI::getPieceValue(Type pieceType) {
    switch (pieceType) {
        case Pawn: return 100;
        case Knight: return 320;
        case Bishop: return 330;
        case Rook: return 500;
        case Queen: return 900;
        case King: return 20000;
        default: return 0;
    }
}

int ChessAI::getPositionBonus(Type pieceType, int row, int col, Color color) {
    static const int pawnTable[8][8] = {
        { 0,  0,  0,  0,  0,  0,  0,  0},
        {50, 50, 50, 50, 50, 50, 50, 50},
        {10, 10, 20, 30, 30, 20, 10, 10},
        { 5,  5, 10, 25, 25, 10,  5,  5},
        { 0,  0,  0, 20, 20,  0,  0,  0},
        { 5, -5,-10,  0,  0,-10, -5,  5},
        { 5, 10, 10,-20,-20, 10, 10,  5},
        { 0,  0,  0,  0,  0,  0,  0,  0}
    };

    static const int knightTable[8][8] = {
        {-50,-40,-30,-30,-30,-30,-40,-50},
        {-40,-20,  0,  0,  0,  0,-20,-40},
        {-30,  0, 10, 15, 15, 10,  0,-30},
        {-30,  5, 15, 20, 20, 15,  5,-30},
        {-30,  0, 15, 20, 20, 15,  0,-30},
        {-30,  5, 10, 15, 15, 10,  5,-30},
        {-40,-20,  0,  5,  5,  0,-20,-40},
        {-50,-40,-30,-30,-30,-30,-40,-50}
    };

    static const int bishopTable[8][8] = {
        {-20,-10,-10,-10,-10,-10,-10,-20},
        {-10,  0,  0,  0,  0,  0,  0,-10},
        {-10,  0,  5, 10, 10,  5,  0,-10},
        {-10,  5,  5, 10, 10,  5,  5,-10},
        {-10,  0, 10, 10, 10, 10,  0,-10},
        {-10, 10, 10, 10, 10, 10, 10,-10},
        {-10,  5,  0,  0,  0,  0,  5,-10},
        {-20,-10,-10,-10,-10,-10,-10,-20}
    };

    static const int rookTable[8][8] = {
        { 0,  0,  0,  0,  0,  0,  0,  0},
        { 5, 10, 10, 10, 10, 10, 10,  5},
        {-5,  0,  0,  0,  0,  0,  0, -5},
        {-5,  0,  0,  0,  0,  0,  0, -5},
        {-5,  0,  0,  0,  0,  0,  0, -5},
        {-5,  0,  0,  0,  0,  0,  0, -5},
        {-5,  0,  0,  0,  0,  0,  0, -5},
        { 0,  0,  0,  5,  5,  0,  0,  0}
    };

    static const int queenTable[8][8] = {
        {-20,-10,-10, -5, -5,-10,-10,-20},
        {-10,  0,  0,  0,  0,  0,  0,-10},
        {-10,  0,  5,  5,  5,  5,  0,-10},
        { -5,  0,  5,  5,  5,  5,  0, -5},
        {  0,  0,  5,  5,  5,  5,  0, -5},
        {-10,  5,  5,  5,  5,  5,  0,-10},
        {-10,  0,  5,  0,  0,  0,  0,-10},
        {-20,-10,-10, -5, -5,-10,-10,-20}
    };

    static const int kingMiddleTable[8][8] = {
        {-30,-40,-40,-50,-50,-40,-40,-30},
        {-30,-40,-40,-50,-50,-40,-40,-30},
        {-30,-40,-40,-50,-50,-40,-40,-30},
        {-30,-40,-40,-50,-50,-40,-40,-30},
        {-20,-30,-30,-40,-40,-30,-30,-20},
        {-10,-20,-20,-20,-20,-20,-20,-10},
        { 20, 20,  0,  0,  0,  0, 20, 20},
        { 20, 30, 10,  0,  0, 10, 30, 20}
    };

    int actualRow = (color == White) ? (7 - row) : row;

    switch (pieceType) {
        case Pawn: return pawnTable[actualRow][col];
        case Knight: return knightTable[actualRow][col];
        case Bishop: return bishopTable[actualRow][col];
        case Rook: return rookTable[actualRow][col];
        case Queen: return queenTable[actualRow][col];
        case King: return kingMiddleTable[actualRow][col];
        default: return 0;
    }
}

int ChessAI::evaluateBoard(ChessBoard& board, Color maximizingColor) {
    int score = 0;

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            ChessPiece* piece = board.getPiece(row, col);
            if (piece != nullptr) {
                int pieceValue = getPieceValue(piece->getType());
                int positionBonus = getPositionBonus(piece->getType(), row, col, piece->getColor());
                int totalValue = pieceValue + positionBonus;

                if (piece->getColor() == maximizingColor) {
                    score += totalValue;
                } else {
                    score -= totalValue;
                }
            }
        }
    }

    return score;
}

std::vector<ChessAI::Move> ChessAI::generateAllMoves(ChessBoard& board, Color color) {
    std::vector<Move> moves;
    Color savedTurn = board.getTurn();
    board.setTurn(color);

    for (int fromRow = 0; fromRow < 8; fromRow++) {
        for (int fromCol = 0; fromCol < 8; fromCol++) {
            ChessPiece* piece = board.getPiece(fromRow, fromCol);
            if (piece && piece->getColor() == color) {
                std::vector<std::pair<int, int>> possibleMoves = board.getPossibleMoves(fromRow, fromCol);
                for (const auto& move : possibleMoves) {
                    moves.push_back(Move(fromRow, fromCol, move.first, move.second));
                }
            }
        }
    }

    board.setTurn(savedTurn);
    return moves;
}

void ChessAI::orderMoves(std::vector<Move>& moves, ChessBoard& board) {
    for (auto& move : moves) {
        move.score = 0;

        ChessPiece* targetPiece = board.getPiece(move.toRow, move.toCol);
        if (targetPiece != nullptr) {
            move.score = 10 * getPieceValue(targetPiece->getType());
            ChessPiece* attackingPiece = board.getPiece(move.fromRow, move.fromCol);
            if (attackingPiece != nullptr) {
                move.score -= getPieceValue(attackingPiece->getType());
            }
        }

        if ((move.toRow == 3 || move.toRow == 4) && (move.toCol == 3 || move.toCol == 4)) {
            move.score += 50;
        }
    }

    std::sort(moves.begin(), moves.end(), [](const Move& a, const Move& b) {
        return a.score > b.score;
    });
}

bool ChessAI::isKingInCheck(ChessBoard& board, Color color) {
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

bool ChessAI::hasValidMoves(ChessBoard& board, Color color) {
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

bool ChessAI::isGameOver(ChessBoard& board, Color currentPlayer) {
    return !hasValidMoves(board, currentPlayer);
}

int ChessAI::minimax(ChessBoard& board, int depth, int alpha, int beta,
                     bool isMaximizingPlayer, Color maximizingColor) {
    Color currentPlayer = isMaximizingPlayer ? maximizingColor :
                         ((maximizingColor == White) ? Black : White);

    if (depth == 0 || isGameOver(board, currentPlayer)) {
        int eval = evaluateBoard(board, maximizingColor);

        if (!hasValidMoves(board, currentPlayer)) {
            if (isKingInCheck(board, currentPlayer)) {
                return isMaximizingPlayer ? -CHECKMATE_SCORE + (MAX_DEPTH - depth) :
                                           CHECKMATE_SCORE - (MAX_DEPTH - depth);
            }
            return 0;
        }
        return eval;
    }

    std::vector<Move> moves = generateAllMoves(board, currentPlayer);
    orderMoves(moves, board);

    static int nodeCount = 0;

    if (isMaximizingPlayer) {
        int maxEval = -INFINITY_SCORE;
        for (size_t i = 0; i < moves.size(); i++) {
            const auto& move = moves[i];

            // Save the piece at the destination (if any) before moving
            ChessPiece* movingPiece = board.getPiece(move.fromRow, move.fromCol);
            ChessPiece* capturedPiece = board.getPiece(move.toRow, move.toCol);

            if (movingPiece == nullptr) continue;

            board.setTurn(currentPlayer);
            if (board.movePiece(move.fromRow, move.fromCol, move.toRow, move.toCol)) {
                int eval = minimax(board, depth - 1, alpha, beta, false, maximizingColor);

                // Undo the move by restoring pieces to original positions
                board.setTurn(currentPlayer);
                board.restorePiece(move.fromRow, move.fromCol, movingPiece);
                board.restorePiece(move.toRow, move.toCol, capturedPiece);

                maxEval = std::max(maxEval, eval);
                alpha = std::max(alpha, eval);
                if (beta <= alpha) {
                    break;
                }
            }

            // Periodically yield to prevent watchdog timeout
            nodeCount++;
            if (nodeCount % 50 == 0) {
                vTaskDelay(1 / portTICK_PERIOD_MS);
            }
        }
        return maxEval;
    } else {
        int minEval = INFINITY_SCORE;
        for (size_t i = 0; i < moves.size(); i++) {
            const auto& move = moves[i];

            // Save the piece at the destination (if any) before moving
            ChessPiece* movingPiece = board.getPiece(move.fromRow, move.fromCol);
            ChessPiece* capturedPiece = board.getPiece(move.toRow, move.toCol);

            if (movingPiece == nullptr) continue;

            board.setTurn(currentPlayer);
            if (board.movePiece(move.fromRow, move.fromCol, move.toRow, move.toCol)) {
                int eval = minimax(board, depth - 1, alpha, beta, true, maximizingColor);

                // Undo the move by restoring pieces to original positions
                board.setTurn(currentPlayer);
                board.restorePiece(move.fromRow, move.fromCol, movingPiece);
                board.restorePiece(move.toRow, move.toCol, capturedPiece);

                minEval = std::min(minEval, eval);
                beta = std::min(beta, eval);
                if (beta <= alpha) {
                    break;
                }
            }

            // Periodically yield to prevent watchdog timeout
            nodeCount++;
            if (nodeCount % 50 == 0) {
                vTaskDelay(1 / portTICK_PERIOD_MS);
            }
        }
        return minEval;
    }
}

ChessAI::Move ChessAI::findBestMove(ChessBoard& board, Color aiColor) {
    Move bestMove;
    int bestScore = -INFINITY_SCORE;
    int alpha = -INFINITY_SCORE;
    int beta = INFINITY_SCORE;

    std::vector<Move> moves = generateAllMoves(board, aiColor);
    orderMoves(moves, board);

    if (moves.empty()) {
        return bestMove;
    }

    for (size_t i = 0; i < moves.size(); i++) {
        const auto& move = moves[i];

        // Save the pieces before moving
        ChessPiece* movingPiece = board.getPiece(move.fromRow, move.fromCol);
        ChessPiece* capturedPiece = board.getPiece(move.toRow, move.toCol);

        if (movingPiece == nullptr) continue;

        board.setTurn(aiColor);
        if (board.movePiece(move.fromRow, move.fromCol, move.toRow, move.toCol)) {
            int score = minimax(board, MAX_DEPTH - 1, alpha, beta, false, aiColor);

            // Undo the move
            board.setTurn(aiColor);
            board.restorePiece(move.fromRow, move.fromCol, movingPiece);
            board.restorePiece(move.toRow, move.toCol, capturedPiece);

            if (score > bestScore) {
                bestScore = score;
                bestMove = move;
            }
            alpha = std::max(alpha, score);
        }

        if (i % 5 == 0) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
    }

    if (bestScore > -CHECKMATE_SCORE && bestScore < -CHECKMATE_SCORE + 100) {
        for (const auto& move : moves) {
            board.setTurn(aiColor);
            if (board.movePiece(move.fromRow, move.fromCol, move.toRow, move.toCol)) {
                board.setTurn(aiColor);
                board.movePiece(move.toRow, move.toCol, move.fromRow, move.fromCol);
                return move;
            }
        }
    }

    return bestMove;
}

}