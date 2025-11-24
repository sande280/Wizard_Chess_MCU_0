#ifndef BOARD_STATE_TRACKER_HH
#define BOARD_STATE_TRACKER_HH

#include "ChessBoard.hh"
#include "Chess.h"

namespace Student {

class BoardStateTracker {
public:
    BoardStateTracker();
    ~BoardStateTracker();

    bool validateStartingPosition(bool occupied[8][8]);

    void showExpectedStartingPosition();

    void initializeTracking(ChessBoard& board, bool occupied[8][8]);

    bool processNextState(ChessBoard& board, bool prevOccupied[8][8], bool newOccupied[8][8], Color& currentPlayer);

private:
    bool detectMove(bool prevOccupied[8][8], bool newOccupied[8][8], int& fromRow, int& fromCol, int& toRow, int& toCol);

    bool detectCapture(bool prevOccupied[8][8], bool newOccupied[8][8], Color playerTurn, int& captureRow, int& captureCol);

    int countDifferences(bool prevOccupied[8][8], bool newOccupied[8][8]);

    bool isStartingPosition(int row, int col);

    Type getPieceTypeFromStartingPosition(int row, int col);

    Color getPieceColorFromStartingPosition(int row, int col);
};

}

#endif
