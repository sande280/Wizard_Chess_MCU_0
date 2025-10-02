#include "BishopPiece.hh"
#include "ChessBoard.hh"
#include <cmath>

namespace Student
{
    BishopPiece::BishopPiece(ChessBoard &board, Color color, int row, int column)
        : ChessPiece(board, color, row, column)
    {
        type = Bishop;
    }

    bool BishopPiece::canMoveToLocation(int toRow, int toColumn)
    {
        if (toRow == row && toColumn == column)
        {
            return false;
        }

        int rowDiff = toRow - row;
        int colDiff = toColumn - column;

        if (abs(rowDiff) != abs(colDiff))
        {
            return false;
        }

        int stepRow = (rowDiff > 0) ? 1 : -1;
        int stepCol = (colDiff > 0) ? 1 : -1;

        int currentRow = row + stepRow;
        int currentCol = column + stepCol;

        while (currentRow != toRow && currentCol != toColumn)
        {
            if (board.getPiece(currentRow, currentCol) != nullptr)
            {
                return false;
            }
            currentRow += stepRow;
            currentCol += stepCol;
        }

        ChessPiece *targetPiece = board.getPiece(toRow, toColumn);
        if (targetPiece != nullptr && targetPiece->getColor() == color)
        {
            return false;
        }

        return true;
    }

    const char *BishopPiece::toString()
    {
        return (color == White) ? "B" : "b";
    }
}
