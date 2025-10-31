#include "QueenPiece.hh"
#include "ChessBoard.hh"
#include "esp_mac.h"
#include <cmath>

namespace Student
{
    QueenPiece::QueenPiece(ChessBoard &board, Color color, int row, int column)
        : ChessPiece(board, color, row, column)
    {
        type = Queen;
    }

    bool QueenPiece::canMoveToLocation(int toRow, int toColumn)
    {
        if (toRow==row && toColumn==column) return false;
        int rDiff=toRow-row;
        int cDiff=toColumn-column;
        if (rDiff!=0 && cDiff!=0 && abs(rDiff)!=abs(cDiff)) return false;

        int stepRow=0;
        int stepCol=0;
        if (rDiff>0) stepRow=1;
        if (rDiff<0) stepRow=-1;
        if (cDiff>0) stepCol=1;
        if (cDiff<0) stepCol=-1;
        int cr=row+stepRow;
        int cc=column+stepCol;
        while(!(cr==toRow && cc==toColumn)) {
            if (board.getPiece(cr,cc)!=nullptr) {
                return false;
            }
            cr+=stepRow;
            cc+=stepCol;
        }
        ChessPiece *tp=board.getPiece(toRow,toColumn);
        if (tp!=nullptr && tp->getColor()==color) return false;
        return true;
    }

    const char *QueenPiece::toString()
    {
        return (color==White)?"Q":"q";
    }
}
