#include "PawnPiece.hh"
#include "ChessBoard.hh"
#include <cmath>

namespace Student
{
    PawnPiece::PawnPiece(ChessBoard &board, Color color, int row, int column)
        : ChessPiece(board, color, row, column)
    {
        type = Pawn;
    }

    bool PawnPiece::canMoveToLocation(int toRow, int toColumn)
    {
        if (toRow == row && toColumn == column)
        {
            return false;
        }

        int direction = (color == White) ? -1 : 1;
        int startRow = (color == White) ? board.getNumRows() - 2 : 1;

        int rowDiff = toRow - row;
        int colDiff = toColumn - column;

        if (colDiff == 0)
        {
            if (rowDiff == direction)
            {
                if (board.getPiece(toRow, toColumn) == nullptr)
                {
                    return true;
                }
            }
            else if (row == startRow && rowDiff == 2 * direction)
            {
                int intermediateRow = row + direction;
                if (board.getPiece(intermediateRow, toColumn) == nullptr && board.getPiece(toRow, toColumn) == nullptr)
                {
                    return true;
                }
            }
        }
        else if (abs(colDiff) == 1 && rowDiff == direction)
        {
            ChessPiece *targetPiece = board.getPiece(toRow, toColumn);
            if (targetPiece != nullptr && targetPiece->getColor() != color)
            {
                return true;
            }
        }

        return false;
    }

    const char *PawnPiece::toString()
    {
        return (color == White) ? "P" : "p";
    }
}
