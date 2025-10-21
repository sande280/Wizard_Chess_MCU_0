#ifndef CHESS_AI_HH
#define CHESS_AI_HH

#include "ChessBoard.hh"
#include "ChessPiece.hh"
#include <vector>
#include <limits>

namespace Student {

class ChessAI {
public:
    struct Move {
        int fromRow, fromCol, toRow, toCol;
        int score;

        Move() : fromRow(-1), fromCol(-1), toRow(-1), toCol(-1), score(0) {}
        Move(int fr, int fc, int tr, int tc)
            : fromRow(fr), fromCol(fc), toRow(tr), toCol(tc), score(0) {}
    };

    static constexpr int MAX_DEPTH = 2;  // 1 turn = 2 ply (minimal depth for ESP32)
    static constexpr int INFINITY_SCORE = 1000000;
    static constexpr int CHECKMATE_SCORE = 100000;

    Move findBestMove(ChessBoard& board, Color aiColor);

private:
    int evaluateBoard(ChessBoard& board, Color maximizingColor);
    int getPieceValue(Type pieceType);
    int getPositionBonus(Type pieceType, int row, int col, Color color);

    std::vector<Move> generateAllMoves(ChessBoard& board, Color color);
    void orderMoves(std::vector<Move>& moves, ChessBoard& board);

    int minimax(ChessBoard& board, int depth, int alpha, int beta,
                bool isMaximizingPlayer, Color maximizingColor);

    bool isGameOver(ChessBoard& board, Color currentPlayer);
    bool isKingInCheck(ChessBoard& board, Color color);
    bool hasValidMoves(ChessBoard& board, Color color);
};

}

#endif