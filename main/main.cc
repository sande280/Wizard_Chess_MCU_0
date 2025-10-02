#include <assert.h>
#include <iostream>
#include "Chess.h"
#include "ChessBoard.hh"
#include "ChessPiece.hh"

using namespace Student;

void test_part1_4x4_1() {
     // Config file content:
    // 0
    // 4 4
    // w r 3 2
    // b b 1 3
    // b r 1 1
    // w r 2 3
    // ~
    // isValidScan

    ChessBoard sBoard(4, 4);
    sBoard.createChessPiece(White, Rook, 3, 2);
    sBoard.createChessPiece(Black, Bishop, 1, 3);
    sBoard.createChessPiece(Black, Rook, 1, 1);
    sBoard.createChessPiece(White, Rook, 2, 3);

    for (int fromRow = 0; fromRow < sBoard.getNumRows(); ++fromRow) {
        for (int fromCol = 0; fromCol < sBoard.getNumCols(); ++fromCol) {
            ChessPiece *piece = sBoard.getPiece(fromRow, fromCol);
            if (piece == nullptr)
                continue;

            for (int toRow = 0; toRow < sBoard.getNumRows(); ++toRow) {
                for (int toCol = 0; toCol < sBoard.getNumCols(); ++toCol) {
                    if (fromRow == toRow && fromCol == toCol)
                        continue; 
                    bool isValid = sBoard.isValidMove(fromRow, fromCol, toRow, toCol);
                    if (isValid) {
                        std::cout << fromRow << " " << fromCol << " " << toRow << " " << toCol << std::endl;
                    }
                }
            }
        }
    }
}

int main()
{
    test_part1_4x4_1();
    return 0;
}
