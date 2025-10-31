#include "KnightPiece.hh"
#include "ChessBoard.hh"
#include <cmath>

namespace Student
{
    KnightPiece::KnightPiece(ChessBoard &board, Color color, int row, int column)
        : ChessPiece(board, color, row, column)
    {
        type = Knight;
    }

    bool KnightPiece::canMoveToLocation(int toRow, int toColumn)
    {
        if (toRow==row && toColumn==column) return false;
        int rDiff=abs(toRow-row);
        int cDiff=abs(toColumn-column);
        if (!((rDiff==2 && cDiff==1)||(rDiff==1 && cDiff==2))) return false;
        ChessPiece *t=board.getPiece(toRow,toColumn);
        if (t!=nullptr && t->getColor()==color) return false;
        return true;
    }

    const char *KnightPiece::toString()
    {
        return (color==White)?"N":"n";
    }
}
