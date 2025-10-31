#ifndef _CHESSPIECE_H__
#define _CHESSPIECE_H__

#include "Chess.h"

namespace Student
{
  class ChessBoard;

  class ChessPiece
  {
    protected:
    ChessBoard &board;
    Color color;
    Type type;
    int row;
    int column;
  public:
    ChessPiece(ChessBoard &board, Color color, int row, int column);
    virtual ~ChessPiece() {}
    Color getColor();
    Type getType();
    int getRow();
    int getColumn();
    virtual void setPosition(int row, int column);
    virtual bool canMoveToLocation(int toRow, int toColumn) = 0;
    virtual const char *toString() = 0;
  };
}

#endif
