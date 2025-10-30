#ifndef CHESS_AI_HH
#define CHESS_AI_HH

#include "ChessBoard.hh"
#include "Chess.h"
#include <utility>
#include <vector>

namespace Student {

struct AIMove {
    int fromRow, fromCol, toRow, toCol;
    float score;

    AIMove() : fromRow(-1), fromCol(-1), toRow(-1), toCol(-1), score(-999999.0f) {}
};

class ChessAI {
public:
    ChessAI();

    AIMove findBestMove(ChessBoard& board, Color aiColor, int depth);

    int getNodesEvaluated() const { return nodesEvaluated; }

private:
    static const float PAWN_TABLE[8][8];
    static const float KNIGHT_TABLE[8][8];
    static const float BISHOP_TABLE[8][8];
    static const float ROOK_TABLE[8][8];
    static const float QUEEN_TABLE[8][8];
    static const float KING_TABLE[8][8];

    float evaluate(ChessBoard& board, Color perspective);

    float getPieceValue(Type type);
    float getPositionBonus(ChessPiece* piece, int row, int col);

    float minimax(ChessBoard& board, int depth, float alpha, float beta,
                  bool maximizing, Color perspective);

    int nodesEvaluated;
};

} // namespace Student

#endif // CHESS_AI_HH
