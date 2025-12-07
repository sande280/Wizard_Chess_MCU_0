#ifndef _CHESSBOARD_H__
#define _CHESSBOARD_H__

#include "ChessPiece.hh"
#include "KingPiece.hh"
#include <list>
#include <vector>
#include <sstream>

namespace Student
{
    class ChessBoard
    {
    private:
        bool kingSafeQ(Color color);
        int numRows = 0;
        int numCols = 0;
        Color turn = White;
        std::vector<std::vector<ChessPiece *>> board;
        std::vector<ChessPiece *> capturedWhitePieces;
        std::vector<ChessPiece *> capturedBlackPieces;
        bool whiteKingMoved = false;
        bool blackKingMoved = false;
        bool whiteRookLeftMoved = false;
        bool whiteRookRightMoved = false;
        bool blackRookLeftMoved = false;
        bool blackRookRightMoved = false;
        bool enP = false;
        int enPRow = -1;
        int enPCol = -1;
    public:
        bool canTake(int row, int col, Color ownColor);
        ChessBoard(int numRow, int numCol);
        ~ChessBoard();
        int getNumRows();
        int getNumCols();
        ChessPiece *getPiece(int r, int c);
        void createChessPiece(Color col, Type ty, int startRow, int startColumn);
        bool movePiece(int fromRow, int fromColumn, int toRow, int toColumn, bool isSimulation = false);
        void restorePiece(int row, int col, ChessPiece* piece);
        void removeCapturedPiece(ChessPiece* piece);
        bool isValidMove(int fromRow, int fromColumn, int toRow, int toColumn);
        bool isValidMoveSimple(int fromRow, int fromColumn, int toRow, int toColumn);
        std::vector<std::pair<int, int>> getPossibleMoves(int fromRow, int fromColumn);
        void setTurn(Color color) { turn = color; }
        Color getTurn() const { return turn; }
        bool isPieceUnderThreat(int row, int column);
        bool isSquareAttacked(int row, int col, Color attacker);
        bool isEnPassantTarget(int row, int col) const;
        bool isKingInCheck(Color color);
        bool hasValidMoves(Color color);
        std::ostringstream displayBoard();
        std::ostringstream displayExpandedBoard();
        float scoreBoard();
        float getHighestNextScore();
        const std::vector<ChessPiece*>& getCapturedWhitePieces() const { return capturedWhitePieces; }
        const std::vector<ChessPiece*>& getCapturedBlackPieces() const { return capturedBlackPieces; }
    };
}

#endif
