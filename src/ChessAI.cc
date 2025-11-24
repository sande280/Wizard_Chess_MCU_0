#include "ChessAI.hh"
#include "ChessPiece.hh"
#include <algorithm>
#include <climits>

namespace Student {




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
        case Bishop: return 3.1f;
        case Rook:   return 5.0f;
        case Queen:  return 9.0f;
        case King:   return 1000.0f;
        default:     return 0.0f;
    }
}

float ChessAI::getPositionBonus(ChessPiece* piece, int row, int col) {

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


    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            ChessPiece* piece = board.getPiece(row, col);
            if (!piece) continue;


            float value = getPieceValue(piece->getType());
            value += getPositionBonus(piece, row, col);


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

    if (depth == 0) {
        nodesEvaluated++;
        return evaluate(board, perspective);
    }

    Color currentPlayer = maximizing ? perspective :
                          (perspective == White ? Black : White);


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


    if (allMoves.empty()) {
        return 0.0f;
    }

    if (maximizing) {
        float maxEval = -999999.0f;

        for (const auto& moveP : allMoves) {
            int fromRow = moveP.first.first;
            int fromCol = moveP.first.second;
            int toRow = moveP.second.first;
            int toCol = moveP.second.second;


            ChessPiece* capturedPiece = board.getPiece(toRow, toCol);
            ChessPiece* movingPiece = board.getPiece(fromRow, fromCol);


            board.setTurn(currentPlayer);
            bool moveSuccess = board.movePiece(fromRow, fromCol, toRow, toCol, true);

            if (!moveSuccess) {

                continue;
            }


            float eval = minimax(board, depth - 1, alpha, beta, false, perspective);


            board.restorePiece(fromRow, fromCol, movingPiece);
            board.restorePiece(toRow, toCol, capturedPiece);
            movingPiece->setPosition(fromRow, fromCol);

            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);
            if (beta <= alpha) {
                break;
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


            ChessPiece* capturedPiece = board.getPiece(toRow, toCol);
            ChessPiece* movingPiece = board.getPiece(fromRow, fromCol);


            board.setTurn(currentPlayer);
            bool moveSuccess = board.movePiece(fromRow, fromCol, toRow, toCol, true);

            if (!moveSuccess) {
                continue;
            }


            float eval = minimax(board, depth - 1, alpha, beta, true, perspective);


            board.restorePiece(fromRow, fromCol, movingPiece);
            board.restorePiece(toRow, toCol, capturedPiece);
            movingPiece->setPosition(fromRow, fromCol);

            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);
            if (beta <= alpha) {
                break;
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


    for (int fromRow = 0; fromRow < 8; fromRow++) {
        for (int fromCol = 0; fromCol < 8; fromCol++) {
            ChessPiece* piece = board.getPiece(fromRow, fromCol);
            if (!piece || piece->getColor() != aiColor) continue;

            std::vector<std::pair<int, int>> possibleMoves = board.getPossibleMoves(fromRow, fromCol);

            for (const auto& move : possibleMoves) {
                int toRow = move.first;
                int toCol = move.second;


                ChessPiece* capturedPiece = board.getPiece(toRow, toCol);
                ChessPiece* movingPiece = board.getPiece(fromRow, fromCol);


                board.setTurn(aiColor);
                bool moveSuccess = board.movePiece(fromRow, fromCol, toRow, toCol, true);

                if (!moveSuccess) {
                    continue;
                }


                float score = minimax(board, depth - 1, alpha, beta, false, aiColor);


                board.restorePiece(fromRow, fromCol, movingPiece);
                board.restorePiece(toRow, toCol, capturedPiece);
                movingPiece->setPosition(fromRow, fromCol);


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

}
