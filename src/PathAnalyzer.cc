#include "PathAnalyzer.hh"
#include "ChessPiece.hh"
#include <cmath>
#include <algorithm>

namespace Student {

bool PathAnalyzer::isPathClear(int fromRow, int fromCol, int toRow, int toCol, ChessBoard& board) {
    int rowDiff = toRow - fromRow;
    int colDiff = toCol - fromCol;

    if (rowDiff == 0 && colDiff == 0) {
        return true;
    }

    if (rowDiff == 0) {
        int step = (colDiff > 0) ? 1 : -1;
        for (int col = fromCol + step; col != toCol; col += step) {
            if (board.getPiece(fromRow, col) != nullptr) {
                return false;
            }
        }
        return true;
    }

    if (colDiff == 0) {
        int step = (rowDiff > 0) ? 1 : -1;
        for (int row = fromRow + step; row != toRow; row += step) {
            if (board.getPiece(row, fromCol) != nullptr) {
                return false;
            }
        }
        return true;
    }

    if (std::abs(rowDiff) == std::abs(colDiff)) {
        int rowStep = (rowDiff > 0) ? 1 : -1;
        int colStep = (colDiff > 0) ? 1 : -1;
        int row = fromRow + rowStep;
        int col = fromCol + colStep;
        while (row != toRow && col != toCol) {
            if (board.getPiece(row, col) != nullptr) {
                return false;
            }
            row += rowStep;
            col += colStep;
        }
        return true;
    }

    return false;
}

bool PathAnalyzer::isKnightLPathClear(int fromRow, int fromCol, int toRow, int toCol, ChessBoard& board) {
    int rowDiff = toRow - fromRow;
    int colDiff = toCol - fromCol;

    if (std::abs(rowDiff) == 2 && std::abs(colDiff) == 1) {
        int rowStep = (rowDiff > 0) ? 1 : -1;
        int midRow1 = fromRow + rowStep;
        int midRow2 = fromRow + 2 * rowStep;

        if (board.getPiece(midRow1, fromCol) != nullptr) {
            return false;
        }
        if (board.getPiece(midRow2, fromCol) != nullptr) {
            return false;
        }
        return true;
    }

    if (std::abs(rowDiff) == 1 && std::abs(colDiff) == 2) {
        int colStep = (colDiff > 0) ? 1 : -1;
        int midCol1 = fromCol + colStep;
        int midCol2 = fromCol + 2 * colStep;

        if (board.getPiece(fromRow, midCol1) != nullptr) {
            return false;
        }
        if (board.getPiece(fromRow, midCol2) != nullptr) {
            return false;
        }
        return true;
    }

    return false;
}

PathType PathAnalyzer::analyzeMovePath(int fromRow, int fromCol, int toRow, int toCol,
                                       ChessBoard& board, Type pieceType, bool isCastling) {
    if (isCastling) {
        return CASTLING_INDIRECT;
    }

    int rowDiff = toRow - fromRow;
    int colDiff = toCol - fromCol;

    if (pieceType == Knight) {
        if (isKnightLPathClear(fromRow, fromCol, toRow, toCol, board)) {
            return DIRECT_L_PATH;
        } else {
            return INDIRECT_L_PATH;
        }
    }

    if (rowDiff == 0) {
        if (isPathClear(fromRow, fromCol, toRow, toCol, board)) {
            return DIRECT_HORIZONTAL;
        } else {
            return INDIRECT_HORIZONTAL;
        }
    }

    if (colDiff == 0) {
        if (isPathClear(fromRow, fromCol, toRow, toCol, board)) {
            return DIRECT_VERTICAL;
        } else {
            return INDIRECT_VERTICAL;
        }
    }

    if (std::abs(rowDiff) == std::abs(colDiff)) {
        if (isPathClear(fromRow, fromCol, toRow, toCol, board)) {
            return DIAGONAL_DIRECT;
        } else {
            return DIAGONAL_INDIRECT;
        }
    }

    return DIRECT_HORIZONTAL;
}

std::string PathAnalyzer::pathTypeToString(PathType type) {
    switch (type) {
        case DIRECT_HORIZONTAL:
            return "Direct (horizontal)";
        case DIRECT_VERTICAL:
            return "Direct (vertical)";
        case DIAGONAL_DIRECT:
            return "Diagonal direct";
        case DIRECT_L_PATH:
            return "Direct (L-path)";
        case INDIRECT_HORIZONTAL:
            return "Indirect (horizontal)";
        case INDIRECT_VERTICAL:
            return "Indirect (vertical)";
        case DIAGONAL_INDIRECT:
            return "Diagonal indirect (staircase pattern)";
        case INDIRECT_L_PATH:
            return "Indirect (L-path along edges)";
        case CASTLING_INDIRECT:
            return "Indirect (castling - pieces moved along edges)";
        default:
            return "Unknown";
    }
}

std::pair<int, int> PathAnalyzer::findCaptureZoneDestination(ChessBoard& board, Color capturedColor) {
    int row, col;

    if (capturedColor == White) {
        size_t count = board.getCapturedWhitePieces().size();
        row = count / 2;
        col = (count % 2) + 10;
    } else {
        size_t count = board.getCapturedBlackPieces().size();
        row = count / 2;
        col = (count % 2);
    }

    return std::make_pair(row, col);
}

} // namespace Student
