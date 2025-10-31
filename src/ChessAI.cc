#include "ChessAI.hh"
#include "ChessPiece.hh"
#include <algorithm>
#include <climits>

namespace Student {

// Position tables - encode chess knowledge

// Pawns: reward center control and advancement
const float ChessAI::PAWN_TABLE[8][8] = {
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5},
    {0.1, 0.1, 0.2, 0.3, 0.3, 0.2, 0.1, 0.1},
    {0.05, 0.05, 0.1, 0.25, 0.25, 0.1, 0.05, 0.05},
    {0.0, 0.0, 0.0, 0.2, 0.2, 0.0, 0.0, 0.0},
    {0.05, -0.05, -0.1, 0.0, 0.0, -0.1, -0.05, 0.05},
    {0.05, 0.1, 0.1, -0.2, -0.2, 0.1, 0.1, 0.05},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

// Knights: penalize edges, reward center
const float ChessAI::KNIGHT_TABLE[8][8] = {
    {-0.5, -0.4, -0.3, -0.3, -0.3, -0.3, -0.4, -0.5},
    {-0.4, -0.2,  0.0,  0.0,  0.0,  0.0, -0.2, -0.4},
    {-0.3,  0.0,  0.1,  0.15, 0.15, 0.1,  0.0, -0.3},
    {-0.3,  0.05, 0.15, 0.2,  0.2,  0.15, 0.05, -0.3},
    {-0.3,  0.0,  0.15, 0.2,  0.2,  0.15, 0.0, -0.3},
    {-0.3,  0.05, 0.1,  0.15, 0.15, 0.1,  0.05, -0.3},
    {-0.4, -0.2,  0.0,  0.05, 0.05, 0.0, -0.2, -0.4},
    {-0.5, -0.4, -0.3, -0.3, -0.3, -0.3, -0.4, -0.5}
};

// Bishops: reward long diagonals
const float ChessAI::BISHOP_TABLE[8][8] = {
    {-0.2, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.2},
    {-0.1,  0.0,  0.0,  0.0,  0.0,  0.0,  0.0, -0.1},
    {-0.1,  0.0,  0.05, 0.1,  0.1,  0.05, 0.0, -0.1},
    {-0.1,  0.05, 0.05, 0.1,  0.1,  0.05, 0.05, -0.1},
    {-0.1,  0.0,  0.1,  0.1,  0.1,  0.1,  0.0, -0.1},
    {-0.1,  0.1,  0.1,  0.1,  0.1,  0.1,  0.1, -0.1},
    {-0.1,  0.05, 0.0,  0.0,  0.0,  0.0,  0.05, -0.1},
    {-0.2, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.2}
};

// Rooks: prefer 7th rank and open files
const float ChessAI::ROOK_TABLE[8][8] = {
    {0.0,  0.0,  0.0,  0.0,  0.0,  0.0,  0.0,  0.0},
    {0.05, 0.1,  0.1,  0.1,  0.1,  0.1,  0.1,  0.05},
    {-0.05, 0.0,  0.0,  0.0,  0.0,  0.0,  0.0, -0.05},
    {-0.05, 0.0,  0.0,  0.0,  0.0,  0.0,  0.0, -0.05},
    {-0.05, 0.0,  0.0,  0.0,  0.0,  0.0,  0.0, -0.05},
    {-0.05, 0.0,  0.0,  0.0,  0.0,  0.0,  0.0, -0.05},
    {-0.05, 0.0,  0.0,  0.0,  0.0,  0.0,  0.0, -0.05},
    {0.0,  0.0,  0.0,  0.05, 0.05, 0.0,  0.0,  0.0}
};

// Queens: slight center preference
const float ChessAI::QUEEN_TABLE[8][8] = {
    {-0.2, -0.1, -0.1, -0.05, -0.05, -0.1, -0.1, -0.2},
    {-0.1,  0.0,  0.0,  0.0,   0.0,   0.0,  0.0, -0.1},
    {-0.1,  0.0,  0.05, 0.05,  0.05,  0.05, 0.0, -0.1},
    {-0.05, 0.0,  0.05, 0.05,  0.05,  0.05, 0.0, -0.05},
    { 0.0,  0.0,  0.05, 0.05,  0.05,  0.05, 0.0, -0.05},
    {-0.1,  0.05, 0.05, 0.05,  0.05,  0.05, 0.0, -0.1},
    {-0.1,  0.0,  0.05, 0.0,   0.0,   0.0,  0.0, -0.1},
    {-0.2, -0.1, -0.1, -0.05, -0.05, -0.1, -0.1, -0.2}
};

// King: stay safe early game (back rank), more active endgame
const float ChessAI::KING_TABLE[8][8] = {
    {-0.3, -0.4, -0.4, -0.5, -0.5, -0.4, -0.4, -0.3},
    {-0.3, -0.4, -0.4, -0.5, -0.5, -0.4, -0.4, -0.3},
    {-0.3, -0.4, -0.4, -0.5, -0.5, -0.4, -0.4, -0.3},
    {-0.3, -0.4, -0.4, -0.5, -0.5, -0.4, -0.4, -0.3},
    {-0.2, -0.3, -0.3, -0.4, -0.4, -0.3, -0.3, -0.2},
    {-0.1, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.1},
    { 0.2,  0.2,  0.0,  0.0,  0.0,  0.0,  0.2,  0.2},
    { 0.2,  0.3,  0.1,  0.0,  0.0,  0.1,  0.3,  0.2}
};

ChessAI::ChessAI() : nodesEvaluated(0) {
}

float ChessAI::getPieceValue(Type type) {
    switch (type) {
        case Pawn:   return 1.0f;
        case Knight: return 3.0f;
        case Bishop: return 3.1f;  // Slightly prefer bishops over knights
        case Rook:   return 5.0f;
        case Queen:  return 9.0f;
        case King:   return 1000.0f;  // King is invaluable
        default:     return 0.0f;
    }
}

float ChessAI::getPositionBonus(ChessPiece* piece, int row, int col) {
    // Flip row for black pieces (they see the board from opposite side)
    int tableRow = (piece->getColor() == White) ? row : (7 - row);

    float bonus = 0.0f;
    switch (piece->getType()) {
        case Pawn:   bonus = PAWN_TABLE[tableRow][col]; break;
        case Knight: bonus = KNIGHT_TABLE[tableRow][col]; break;
        case Bishop: bonus = BISHOP_TABLE[tableRow][col]; break;
        case Rook:   bonus = ROOK_TABLE[tableRow][col]; break;
        case Queen:  bonus = QUEEN_TABLE[tableRow][col]; break;
        case King:   bonus = KING_TABLE[tableRow][col]; break;
        default:     bonus = 0.0f;
    }

    return bonus;
}

float ChessAI::evaluate(ChessBoard& board, Color perspective) {
    float score = 0.0f;

    // Single pass through board - O(64) complexity
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            ChessPiece* piece = board.getPiece(row, col);
            if (!piece) continue;

            // Get base value + position bonus
            float value = getPieceValue(piece->getType());
            value += getPositionBonus(piece, row, col);

            // Add or subtract based on whose perspective
            if (piece->getColor() == perspective) {
                score += value;
            } else {
                score -= value;
            }
        }
    }

    return score;
}

float ChessAI::minimax(ChessBoard& board, int depth, float alpha, float beta,
                       bool maximizing, Color perspective) {
    // Terminal node: evaluate position
    if (depth == 0) {
        nodesEvaluated++;
        return evaluate(board, perspective);
    }

    Color currentPlayer = maximizing ? perspective :
                          (perspective == White ? Black : White);

    // Generate all legal moves for current player
    std::vector<std::pair<std::pair<int, int>, std::pair<int, int>>> allMoves;

    for (int fromRow = 0; fromRow < 8; fromRow++) {
        for (int fromCol = 0; fromCol < 8; fromCol++) {
            ChessPiece* piece = board.getPiece(fromRow, fromCol);
            if (!piece || piece->getColor() != currentPlayer) continue;

            std::vector<std::pair<int, int>> possibleMoves = board.getPossibleMoves(fromRow, fromCol);

            for (const auto& move : possibleMoves) {
                allMoves.push_back(std::make_pair(
                    std::make_pair(fromRow, fromCol),
                    std::make_pair(move.first, move.second)
                ));
            }
        }
    }

    // No legal moves - stalemate or checkmate (simplified)
    if (allMoves.empty()) {
        return 0.0f;  // Treat as draw for now
    }

    if (maximizing) {
        float maxEval = -999999.0f;

        for (const auto& moveP : allMoves) {
            int fromRow = moveP.first.first;
            int fromCol = moveP.first.second;
            int toRow = moveP.second.first;
            int toCol = moveP.second.second;

            // Save state (capture piece if present)
            ChessPiece* capturedPiece = board.getPiece(toRow, toCol);
            ChessPiece* movingPiece = board.getPiece(fromRow, fromCol);

            // Make move
            board.setTurn(currentPlayer);
            bool moveSuccess = board.movePiece(fromRow, fromCol, toRow, toCol);

            if (!moveSuccess) {
                // Move failed (shouldn't happen with getPossibleMoves), skip
                continue;
            }

            // Recurse
            float eval = minimax(board, depth - 1, alpha, beta, false, perspective);

            // Undo move
            board.restorePiece(fromRow, fromCol, movingPiece);
            if (capturedPiece != nullptr) {
                board.removeCapturedPiece(capturedPiece);
                board.restorePiece(toRow, toCol, capturedPiece);
            } else {
                board.restorePiece(toRow, toCol, nullptr);
            }
            movingPiece->setPosition(fromRow, fromCol);

            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);
            if (beta <= alpha) {
                break;  // Beta cutoff
            }
        }
        return maxEval;

    } else {
        float minEval = 999999.0f;

        for (const auto& moveP : allMoves) {
            int fromRow = moveP.first.first;
            int fromCol = moveP.first.second;
            int toRow = moveP.second.first;
            int toCol = moveP.second.second;

            // Save state
            ChessPiece* capturedPiece = board.getPiece(toRow, toCol);
            ChessPiece* movingPiece = board.getPiece(fromRow, fromCol);

            // Make move
            board.setTurn(currentPlayer);
            bool moveSuccess = board.movePiece(fromRow, fromCol, toRow, toCol);

            if (!moveSuccess) {
                continue;
            }

            // Recurse
            float eval = minimax(board, depth - 1, alpha, beta, true, perspective);

            // Undo move
            board.restorePiece(fromRow, fromCol, movingPiece);
            if (capturedPiece != nullptr) {
                board.removeCapturedPiece(capturedPiece);
                board.restorePiece(toRow, toCol, capturedPiece);
            } else {
                board.restorePiece(toRow, toCol, nullptr);
            }
            movingPiece->setPosition(fromRow, fromCol);

            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);
            if (beta <= alpha) {
                break;  // Alpha cutoff
            }
        }
        return minEval;
    }
}

AIMove ChessAI::findBestMove(ChessBoard& board, Color aiColor, int depth) {
    AIMove bestMove;
    nodesEvaluated = 0;

    float alpha = -999999.0f;
    float beta = 999999.0f;

    // Generate all legal moves for AI
    for (int fromRow = 0; fromRow < 8; fromRow++) {
        for (int fromCol = 0; fromCol < 8; fromCol++) {
            ChessPiece* piece = board.getPiece(fromRow, fromCol);
            if (!piece || piece->getColor() != aiColor) continue;

            std::vector<std::pair<int, int>> possibleMoves = board.getPossibleMoves(fromRow, fromCol);

            for (const auto& move : possibleMoves) {
                int toRow = move.first;
                int toCol = move.second;

                // Save state
                ChessPiece* capturedPiece = board.getPiece(toRow, toCol);
                ChessPiece* movingPiece = board.getPiece(fromRow, fromCol);

                // Make move
                board.setTurn(aiColor);
                bool moveSuccess = board.movePiece(fromRow, fromCol, toRow, toCol);

                if (!moveSuccess) {
                    continue;
                }

                // Evaluate with minimax (opponent's turn next, so minimizing)
                float score = minimax(board, depth - 1, alpha, beta, false, aiColor);

                // Undo move
                board.restorePiece(fromRow, fromCol, movingPiece);
                if (capturedPiece != nullptr) {
                    board.removeCapturedPiece(capturedPiece);
                    board.restorePiece(toRow, toCol, capturedPiece);
                } else {
                    board.restorePiece(toRow, toCol, nullptr);
                }
                movingPiece->setPosition(fromRow, fromCol);

                // Track best move
                if (score > bestMove.score) {
                    bestMove.fromRow = fromRow;
                    bestMove.fromCol = fromCol;
                    bestMove.toRow = toRow;
                    bestMove.toCol = toCol;
                    bestMove.score = score;
                }

                alpha = std::max(alpha, score);
            }
        }
    }

    return bestMove;
}

} // namespace Student
