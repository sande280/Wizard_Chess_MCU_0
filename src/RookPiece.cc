#include "RookPiece.hh"
#include "ChessBoard.hh"

namespace Student
{
    RookPiece::RookPiece(ChessBoard &board, Color color, int row, int column)
        : ChessPiece(board, color, row, column)
    {
        type = Rook;
    }

    bool RookPiece::canMoveToLocation(int toRow, int toColumn)
    {
        if (toRow == row && toColumn == column)
        {
            return false;
        }

        int rowDiff = toRow - row;
        int colDiff = toColumn - column;

        if (rowDiff != 0 && colDiff != 0)
        {
            return false;
        }

        int stepRow = (rowDiff == 0) ? 0 : (rowDiff > 0 ? 1 : -1);
        int stepCol = (colDiff == 0) ? 0 : (colDiff > 0 ? 1 : -1);

        int currentRow = row + stepRow;
        int currentCol = column + stepCol;

        while (currentRow != toRow || currentCol != toColumn)
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

    const char *RookPiece::toString()
    {
        return (color == White) ? "R" : "r";
    }
}
