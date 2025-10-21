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
        std::vector<ChessPiece *> capturedWhitePieces;  // White pieces captured by Black
        std::vector<ChessPiece *> capturedBlackPieces;  // Black pieces captured by White
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
        bool movePiece(int fromRow, int fromColumn, int toRow, int toColumn);
        bool isValidMove(int fromRow, int fromColumn, int toRow, int toColumn);
        bool isValidMoveSimple(int fromRow, int fromColumn, int toRow, int toColumn);
        std::vector<std::pair<int, int>> getPossibleMoves(int fromRow, int fromColumn);
        void setTurn(Color color) { turn = color; }
        Color getTurn() const { return turn; }
        bool isPieceUnderThreat(int row, int column);
        std::ostringstream displayBoard();
        std::ostringstream displayExpandedBoard();  // New method for 12x8 display
        float scoreBoard();
        float getHighestNextScore();
    };
}

#endif
