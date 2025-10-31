#include "KingPiece.hh"
#include "ChessBoard.hh"
#include <cmath>

namespace Student {
    KingPiece::KingPiece(ChessBoard &board, Color color, int row, int column)
        : ChessPiece(board, color, row, column) {
        type = King;
    }

    bool KingPiece::canMoveToLocation(int toRow, int toColumn) {
        int rowDiff = abs(toRow - row);
        int colDiff = abs(toColumn - column);

        if ((abs(rowDiff) > 1) || (abs(colDiff) > 1) || (rowDiff == 0 && colDiff == 0)) {
            int rDiff=toRow-row;
            int cDiff=toColumn-column;
            if (rDiff==0 && abs(cDiff)==2) {
                return true;
            }
            return false;
        }

        ChessPiece *targetPiece = board.getPiece(toRow, toColumn);
        if (targetPiece != nullptr && targetPiece->getColor() == color) {
            return false;
        }

        return true;
    }

    const char *KingPiece::toString() {
        return((color == White) ? "K" : "k");
    }
}
