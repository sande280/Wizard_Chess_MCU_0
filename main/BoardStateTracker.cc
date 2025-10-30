#include "BoardStateTracker.hh"
#include "ChessPiece.hh"
#include "PathAnalyzer.hh"
#include <cstdio>
#include <vector>
#include <utility>

extern void uart_printf(const char* format, ...);

namespace Student {

BoardStateTracker::BoardStateTracker() {
}

BoardStateTracker::~BoardStateTracker() {
}

bool BoardStateTracker::isStartingPosition(int row, int col) {
    if (row == 0 || row == 1 || row == 6 || row == 7) {
        return true;
    }
    return false;
}

Type BoardStateTracker::getPieceTypeFromStartingPosition(int row, int col) {
    if (row == 1 || row == 6) {
        return Pawn;
    }

    if (row == 0 || row == 7) {
        switch (col) {
            case 0:
            case 7:
                return Rook;
            case 1:
            case 6:
                return Knight;
            case 2:
            case 5:
                return Bishop;
            case 3:
                return Queen;
            case 4:
                return King;
            default:
                return Pawn;
        }
    }

    return Pawn;
}

Color BoardStateTracker::getPieceColorFromStartingPosition(int row, int col) {
    if (row == 0 || row == 1) {
        return Black;
    }
    return White;
}

bool BoardStateTracker::validateStartingPosition(bool occupied[8][8]) {
    int count = 0;

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            bool shouldBeOccupied = isStartingPosition(row, col);

            if (shouldBeOccupied) {
                if (!occupied[row][col]) {
                    return false;
                }
                count++;
            } else {
                if (occupied[row][col]) {
                    return false;
                }
            }
        }
    }

    return count == 32;
}

void BoardStateTracker::showExpectedStartingPosition() {
    uart_printf("\r\nExpected starting position for Board_State_0:\r\n\r\n");
    uart_printf("    2 3 4 5 6 7 8 9\r\n");
    uart_printf("    ----------------\r\n");

    const char* pieces[8][8] = {
        {"R", "N", "B", "Q", "K", "B", "N", "R"},
        {"P", "P", "P", "P", "P", "P", "P", "P"},
        {" ", " ", " ", " ", " ", " ", " ", " "},
        {" ", " ", " ", " ", " ", " ", " ", " "},
        {" ", " ", " ", " ", " ", " ", " ", " "},
        {" ", " ", " ", " ", " ", " ", " ", " "},
        {"P", "P", "P", "P", "P", "P", "P", "P"},
        {"R", "N", "B", "Q", "K", "B", "N", "R"}
    };

    const char* colors[8] = {"Black", "Black", "", "", "", "", "White", "White"};

    for (int row = 0; row < 8; row++) {
        uart_printf("  %d|", row);
        for (int col = 0; col < 8; col++) {
            uart_printf(" %s", pieces[row][col]);
        }
        uart_printf(" |");
        if (colors[row][0] != '\0') {
            uart_printf(" %s", colors[row]);
        }
        uart_printf("\r\n");
    }

    uart_printf("    ----------------\r\n\r\n");
}

void BoardStateTracker::initializeTracking(ChessBoard& board, bool occupied[8][8]) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (occupied[row][col]) {
                Color color = getPieceColorFromStartingPosition(row, col);
                Type type = getPieceTypeFromStartingPosition(row, col);
                board.createChessPiece(color, type, row, col);
            }
        }
    }
}

int BoardStateTracker::countDifferences(bool prevOccupied[8][8], bool newOccupied[8][8]) {
    int differences = 0;
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (prevOccupied[row][col] != newOccupied[row][col]) {
                differences++;
            }
        }
    }
    return differences;
}

bool BoardStateTracker::detectMove(bool prevOccupied[8][8], bool newOccupied[8][8], int& fromRow, int& fromCol, int& toRow, int& toCol) {
    fromRow = fromCol = toRow = toCol = -1;
    int emptyCount = 0;
    int filledCount = 0;

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (prevOccupied[row][col] && !newOccupied[row][col]) {
                if (fromRow == -1) {
                    fromRow = row;
                    fromCol = col;
                    emptyCount++;
                } else {
                    return false;
                }
            }

            if (!prevOccupied[row][col] && newOccupied[row][col]) {
                if (toRow == -1) {
                    toRow = row;
                    toCol = col;
                    filledCount++;
                } else {
                    return false;
                }
            }
        }
    }

    if (fromRow != -1 && toRow != -1) {
        return true;
    }

    if (fromRow != -1 && toRow == -1) {
        return true;
    }

    return false;
}

bool BoardStateTracker::detectCapture(bool prevOccupied[8][8], bool newOccupied[8][8], Color playerTurn, int& captureRow, int& captureCol) {
    captureRow = captureCol = -1;
    return false;
}

bool BoardStateTracker::processNextState(ChessBoard& board, bool prevOccupied[8][8], bool newOccupied[8][8], Color& currentPlayer) {
    int fromRow, fromCol, toRow, toCol;

    if (!detectMove(prevOccupied, newOccupied, fromRow, fromCol, toRow, toCol)) {
        int diff = countDifferences(prevOccupied, newOccupied);
        if (diff == 0) {
            uart_printf("Error: No move detected between states.\r\n");
        } else {
            uart_printf("Board state error illegal move\r\n");
        }
        return false;
    }

    ChessPiece* piece = board.getPiece(fromRow, fromCol);
    if (!piece) {
        uart_printf("Board state error illegal move\r\n");
        return false;
    }

    if (piece->getColor() != currentPlayer) {
        uart_printf("Board state error illegal move\r\n");
        return false;
    }

    if (toRow == -1) {
        board.setTurn(currentPlayer);
        std::vector<std::pair<int, int>> possibleMoves = board.getPossibleMoves(fromRow, fromCol);

        bool foundDestination = false;
        for (const auto& move : possibleMoves) {
            int testRow = move.first;
            int testCol = move.second;

            if (prevOccupied[testRow][testCol] && newOccupied[testRow][testCol]) {
                ChessPiece* targetPiece = board.getPiece(testRow, testCol);
                if (targetPiece && targetPiece->getColor() != currentPlayer) {
                    toRow = testRow;
                    toCol = testCol;
                    foundDestination = true;
                    break;
                }
            }
        }

        if (!foundDestination) {
            uart_printf("Board state error illegal move\r\n");
            return false;
        }
    }

    ChessPiece* targetPiece = board.getPiece(toRow, toCol);
    bool isCapture = (targetPiece != nullptr);

    board.setTurn(currentPlayer);

    if (!board.isValidMove(fromRow, fromCol, toRow, toCol)) {
        uart_printf("Board state error illegal move\r\n");
        return false;
    }

    const char* pieceType =
        piece->getType() == Pawn ? "Pawn" :
        piece->getType() == Rook ? "Rook" :
        piece->getType() == Knight ? "Knight" :
        piece->getType() == Bishop ? "Bishop" :
        piece->getType() == Queen ? "Queen" : "King";

    if (isCapture) {
        const char* targetType =
            targetPiece->getType() == Pawn ? "Pawn" :
            targetPiece->getType() == Rook ? "Rook" :
            targetPiece->getType() == Knight ? "Knight" :
            targetPiece->getType() == Bishop ? "Bishop" :
            targetPiece->getType() == Queen ? "Queen" : "King";

        uart_printf("%s %s at (%d,%d) captures %s %s at (%d,%d)\r\n",
                   currentPlayer == White ? "White" : "Black",
                   pieceType,
                   fromRow, fromCol + 2,
                   currentPlayer == White ? "Black" : "White",
                   targetType,
                   toRow, toCol + 2);
    } else {
        uart_printf("%s %s moves from (%d,%d) to (%d,%d)\r\n",
                   currentPlayer == White ? "White" : "Black",
                   pieceType,
                   fromRow, fromCol + 2,
                   toRow, toCol + 2);
    }

    if (board.movePiece(fromRow, fromCol, toRow, toCol)) {
        if (isCapture && targetPiece) {
            auto captureDestination = PathAnalyzer::findCaptureZoneDestination(board, targetPiece->getColor());
            PathType capturePath = PathAnalyzer::analyzeMovePath(toRow, toCol, captureDestination.first, captureDestination.second, board, targetPiece->getType(), false);
            PathType attackPath = PathAnalyzer::analyzeMovePath(fromRow, fromCol, toRow, toCol, board, piece->getType(), false);

            uart_printf("Captured piece path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(capturePath).c_str(),
                       toRow, toCol + 2, captureDestination.first, captureDestination.second + 2);
            uart_printf("Attacking piece path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(attackPath).c_str(),
                       fromRow, fromCol + 2, toRow, toCol + 2);
        } else {
            PathType movePath = PathAnalyzer::analyzeMovePath(fromRow, fromCol, toRow, toCol, board, piece->getType(), false);
            uart_printf("Movement path: %s from (%d,%d) to (%d,%d)\r\n",
                       PathAnalyzer::pathTypeToString(movePath).c_str(),
                       fromRow, fromCol + 2, toRow, toCol + 2);
        }

        currentPlayer = (currentPlayer == White) ? Black : White;
        return true;
    } else {
        uart_printf("Board state error illegal move\r\n");
        return false;
    }
}

} // namespace Student
