// ChessBoard.cc
#include "ChessBoard.hh"
#include "PawnPiece.hh"
#include "RookPiece.hh"
#include "BishopPiece.hh"
#include "KingPiece.hh"
#include "KnightPiece.hh"
#include "QueenPiece.hh"
#include <cmath>

namespace Student
{
    ChessBoard::ChessBoard(int numRow, int numCol)
        : numRows(numRow), numCols(numCol)
    {
        board = std::vector<std::vector<ChessPiece *>>(numRows, std::vector<ChessPiece *>(numCols, nullptr));
    }

    ChessBoard::~ChessBoard()
    {
        int row=0;
        while(row<numRows) {
            int col=0;
            while(col<numCols) {
                if (board[row][col] != nullptr)
                {
                    delete board[row][col];
                    board[row][col] = nullptr;
                }
                col++;
            }
            row++;
        }
    }

    int ChessBoard::getNumRows()
    {
        return numRows;
    }

    int ChessBoard::getNumCols()
    {
        return numCols;
    }

    ChessPiece *ChessBoard::getPiece(int r, int c)
    {
        return board.at(r).at(c);
    }

    void ChessBoard::createChessPiece(Color col, Type ty, int startRow, int startColumn)
    {
        if (board.at(startRow).at(startColumn) != nullptr)
        {
            delete board.at(startRow).at(startColumn);
            board.at(startRow).at(startColumn) = nullptr;
        }

        ChessPiece *newPiece = nullptr;
        switch (ty)
        {
        case Pawn:
            newPiece = new PawnPiece(*this, col, startRow, startColumn);
            break;
        case Rook:
            newPiece = new RookPiece(*this, col, startRow, startColumn);
            break;
        case Bishop:
            newPiece = new BishopPiece(*this, col, startRow, startColumn);
            break;
        case King:
            newPiece = new KingPiece(*this, col, startRow, startColumn);
            break;
        case Knight:
            newPiece = new KnightPiece(*this, col, startRow, startColumn);
            break;
        case Queen:
            newPiece = new QueenPiece(*this, col, startRow, startColumn);
            break;
        default:
            break;
        }

        board.at(startRow).at(startColumn) = newPiece;
    }

    bool ChessBoard::kingSafeQ(Color color)
    {
        int kingRow = -1, kingCol = -1;
        int row=0;
        while(row<numRows) {
            int col=0;
            while(col<numCols) {
                ChessPiece *piece = getPiece(row, col);
                if (piece != nullptr && piece->getColor() == color && piece->getType() == King)
                {
                    kingRow = row;
                    kingCol = col;
                    break;
                }
                col++;
            }
            if (kingRow != -1)
            {
                break;
            }
            row++;
        }
        if (kingRow == -1)
        {
            return false;
        }
        return isPieceUnderThreat(kingRow, kingCol);
    }

    bool ChessBoard::isValidMove(int fromRow, int fromColumn, int toRow, int toColumn)
    {
        if (fromRow < 0 || fromRow >= numRows || fromColumn < 0 || fromColumn >= numCols ||
            toRow < 0 || toRow >= numRows || toColumn < 0 || toColumn >= numCols)
        {
            return false;
        }
        ChessPiece *piece = getPiece(fromRow, fromColumn);
        if (piece == nullptr)
        {
            return false;
        }
        if (fromRow == toRow && fromColumn == toColumn)
        {
            return false;
        }

        if (piece->getType() == King)
        {
            int cDiff = toColumn - fromColumn;
            int rDiff = toRow - fromRow;
            if (std::abs(cDiff) == 2 && rDiff == 0)
            {
                Color cCol = piece->getColor();
                bool kMoved = (cCol == White) ? whiteKingMoved : blackKingMoved;
                bool rSide = (cDiff > 0);
                bool rMoved = false;
                if (numCols<2) return false;
                int rCol = rSide?(numCols-1):0;
                ChessPiece *rr = getPiece(fromRow, rCol);
                if (rr == nullptr || rr->getType() != Rook || rr->getColor() != cCol)
                {
                    return false;
                }
                if (cCol == White)
                {
                    rMoved = rSide ? whiteRookRightMoved : whiteRookLeftMoved;
                }
                else
                {
                    rMoved = rSide ? blackRookRightMoved : blackRookLeftMoved;
                }
                if (kMoved || rMoved)
                {
                    return false;
                }
                int step = (cDiff > 0) ? 1 : -1;
                int cCheck = fromColumn+step;
                while(cCheck != rCol) {
                    if (getPiece(fromRow, cCheck) != nullptr && cCheck != rCol)
                    {
                        return false;
                    }
                    cCheck += step;
                }
                if (kingSafeQ(cCol))
                {
                    return false;
                }
                int interCol = fromColumn+step;
                int finalCol = fromColumn+2*step;
                if (isPieceUnderThreat(fromRow, interCol)) return false;
                if (isPieceUnderThreat(fromRow, finalCol)) return false;
                return true;
            }
        }

        if (!piece->canMoveToLocation(toRow, toColumn))
        {
            if (piece->getType() == Pawn && enP)
            {
                if (toRow == enPRow && toColumn == enPCol && piece->getRow() == ((piece->getColor()==White)?(enPRow+1):(enPRow-1)) && std::abs(piece->getColumn()-enPCol)==1)
                {
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }

        ChessPiece *capturedPiece = getPiece(toRow, toColumn);
        bool doEnP = false;
        ChessPiece *enPCap = nullptr;
        if (piece->getType() == Pawn && enP)
        {
            if (toRow == enPRow && toColumn == enPCol)
            {
                doEnP = true;
                int rp = (piece->getColor()==White) ? enPRow+1 : enPRow-1;
                enPCap = getPiece(rp, enPCol);
                board[fromRow][fromColumn] = nullptr;
                board[toRow][toColumn] = piece;
                board[rp][enPCol] = nullptr;
                piece->setPosition(toRow, toColumn);
            }
        }
        if (!doEnP)
        {
            board[fromRow][fromColumn] = nullptr;
            board[toRow][toColumn] = piece;
            piece->setPosition(toRow, toColumn);
        }

        Color ccolor = piece->getColor();
        bool kingNSafe = kingSafeQ(ccolor);

        if (!doEnP)
        {
            board[fromRow][fromColumn] = piece;
            board[toRow][toColumn] = capturedPiece;
            piece->setPosition(fromRow, fromColumn);
        }
        else
        {
            int rp = (piece->getColor()==White) ? enPRow+1 : enPRow-1;
            board[fromRow][fromColumn] = piece;
            board[toRow][toColumn] = capturedPiece;
            board[rp][enPCol] = enPCap;
            piece->setPosition(fromRow, fromColumn);
        }

        if (kingNSafe)
        {
            return false;
        }
        return true;
    }

    bool ChessBoard::movePiece(int fromRow, int fromColumn, int toRow, int toColumn)
    {
        ChessPiece *pieceM = getPiece(fromRow, fromColumn);
        if(pieceM == nullptr) {
            return(false);
        }
        if(pieceM->getColor() != turn) {
            return(false);
        }

        if(!isValidMove(fromRow, fromColumn, toRow, toColumn)) {
            return(false);
        } else {
            bool cstl = false;
            if (pieceM->getType()==King && std::abs(toColumn - fromColumn)==2 && toRow==fromRow)
            {
                cstl=true;
            }
            ChessPiece *victim = nullptr;
            bool doEnP = false;
            ChessPiece *enPCap=nullptr;
            if (pieceM->getType()==Pawn && enP)
            {
                if (toRow==enPRow && toColumn==enPCol)
                {
                    doEnP = true;
                    int rp=(pieceM->getColor()==White?(enPRow+1):(enPRow-1));
                    enPCap = getPiece(rp,enPCol);
                }
            }
            if (!doEnP && !cstl)
            {
                victim = getPiece(toRow, toColumn);
                if(victim != nullptr) {
                    delete(victim);
                }
                pieceM->setPosition(toRow, toColumn);
                board[fromRow][fromColumn] = nullptr;
                board[toRow][toColumn] = pieceM;
            }
            else if (doEnP)
            {
                int rp=(pieceM->getColor()==White?(enPRow+1):(enPRow-1));
                enPCap = getPiece(rp,enPCol);
                if (enPCap!=nullptr) delete(enPCap);
                board[rp][enPCol]=nullptr;
                pieceM->setPosition(toRow, toColumn);
                board[fromRow][fromColumn]=nullptr;
                board[toRow][toColumn]=pieceM;
            }
            else if (cstl)
            {
                victim = getPiece(toRow,toColumn);
                if(victim!=nullptr) {delete victim;}
                pieceM->setPosition(toRow,toColumn);
                board[fromRow][fromColumn]=nullptr;
                board[toRow][toColumn]=pieceM;
                Color ccol=pieceM->getColor();
                bool rSide=(toColumn>fromColumn);
                int rCol=(rSide?(numCols-1):0);
                ChessPiece *rr=getPiece(fromRow,rCol);
                int step=(rSide?1:-1);
                int nRCol=fromColumn+step;
                ChessPiece* vv=getPiece(fromRow,nRCol);
                if(vv!=nullptr) {delete vv;}
                rr->setPosition(fromRow,nRCol);
                board[fromRow][nRCol]=rr;
                board[fromRow][rCol]=nullptr;
                if (ccol==White) whiteKingMoved=true; else blackKingMoved=true;
                if (ccol==White)
                {
                    if (rSide) whiteRookRightMoved=true; else whiteRookLeftMoved=true;
                }
                else
                {
                    if (rSide) blackRookRightMoved=true; else blackRookLeftMoved=true;
                }
            }

            if (pieceM->getType()==Pawn)
            {
                int startRow=(pieceM->getColor()==White?(getNumRows()-2):1);
                if (std::abs(toRow-fromRow)==2 && fromRow==startRow)
                {
                    enP=true;
                    enPRow=(pieceM->getColor()==White?(toRow+1):(toRow-1));
                    enPCol=toColumn;
                }
                else
                {
                    enP=false;
                }
                if ((pieceM->getColor()==White && toRow==0)||(pieceM->getColor()==Black && toRow==getNumRows()-1))
                {
                    Color pColor = pieceM->getColor();
                    ChessPiece* oldp=getPiece(toRow,toColumn);
                    if (oldp!=nullptr) {
                        delete oldp;
                        board[toRow][toColumn]=nullptr;
                    }
                    createChessPiece(pColor,Queen,toRow,toColumn);
                }
            }
            else
            {
                enP=false;
            }

            if (pieceM->getType()==King)
            {
                if (pieceM->getColor()==White) whiteKingMoved=true; else blackKingMoved=true;
            }
            if (pieceM->getType()==Rook)
            {
                if (pieceM->getColor()==White)
                {
                    if (fromColumn==0 && fromRow==getNumRows()-1) whiteRookLeftMoved=true;
                    if (fromColumn==getNumCols()-1 && fromRow==getNumRows()-1) whiteRookRightMoved=true;
                }
                else
                {
                    if (fromColumn==0 && fromRow==0) blackRookLeftMoved=true;
                    if (fromColumn==getNumCols()-1 && fromRow==0) blackRookRightMoved=true;
                }
            }

            if(turn == White) {
                turn = Black;
            } else {
                turn = White;
            }
            return(true);
        }

        return(false);
    }

    bool ChessBoard::isPieceUnderThreat(int row, int column)
    {
        ChessPiece *tPiece = getPiece(row, column);
        if (tPiece == nullptr)
        {
            return false;
        }
        Color enemy = (tPiece->getColor() == White) ? Black : White;
        int ri=0;
        while(ri<numRows) {
            int ci=0;
            while(ci<numCols) {
                ChessPiece *checkPiece = getPiece(ri, ci);
                if (checkPiece == nullptr)
                {
                    ci++;
                    continue;
                }
                if (checkPiece->getColor() == enemy)
                {
                    if (checkPiece->getType() == Pawn)
                    {
                        int direction = (enemy == White) ? -1 : 1;
                        int rowDiff = row - ri;
                        int colDiff = column - ci;
                        if (rowDiff == direction && std::abs(colDiff) == 1)
                        {
                            return true;
                        }
                    }
                    else
                    {
                        if (checkPiece->canMoveToLocation(row, column))
                        {
                            return true;
                        }
                    }
                }
                ci++;
            }
            ri++;
        }
        return false;
    }

    std::ostringstream ChessBoard::displayBoard()
    {
        std::ostringstream outputString;
        outputString << "  ";
        int i=0;
        while(i<numCols) {
            outputString << i;
            i++;
        }
        outputString << std::endl
                     << "  ";
        int j=0;
        while(j<numCols) {
            outputString << "-";
            j++;
        }
        outputString << std::endl;

        int row=0;
        while(row<numRows) {
            outputString << row << "|";
            int column=0;
            while(column<numCols) {
                ChessPiece *piece = board.at(row).at(column);
                outputString << (piece == nullptr ? " " : piece->toString());
                column++;
            }
            outputString << "|" << std::endl;
            row++;
        }

        outputString << "  ";
        int k=0;
        while(k<numCols) {
            outputString << "-";
            k++;
        }
        outputString << std::endl
                     << std::endl;

        return outputString;
    }

    float ChessBoard::scoreBoard()
    {
        float myScore=0.0f;
        float opScore=0.0f;
        Color me=turn;
        Color op=(me==White)?Black:White;
        int r=0;
        while(r<numRows) {
            int c=0;
            while(c<numCols) {
                ChessPiece *p=getPiece(r,c);
                if (p!=nullptr) {
                    int val=0;
                    if (p->getType()==King) val=200;
                    else if (p->getType()==Queen) val=9;
                    else if (p->getType()==Rook) val=5;
                    else if (p->getType()==Bishop || p->getType()==Knight) val=3;
                    else if (p->getType()==Pawn) val=1;
                    if (p->getColor()==me) myScore+=val; else opScore+=val;
                }
                c++;
            }
            r++;
        }
        int rr=0;
        while(rr<numRows) {
            int cc=0;
            while(cc<numCols) {
                ChessPiece *pp=getPiece(rr,cc);
                if (pp!=nullptr && pp->getColor()==me) {
                    int r2=0;
                    while(r2<numRows) {
                        int c2=0;
                        while(c2<numCols) {
                            if (!(r2==rr && c2==cc)) {
                                bool mv=isValidMove(rr,cc,r2,c2);
                                if (mv) myScore+=0.1f;
                            }
                            c2++;
                        }
                        r2++;
                    }
                }
                cc++;
            }
            rr++;
        }
        int rrr=0;
        while(rrr<numRows) {
            int ccc=0;
            while(ccc<numCols) {
                ChessPiece *pp2=getPiece(rrr,ccc);
                if (pp2!=nullptr && pp2->getColor()==op) {
                    int r3=0;
                    while(r3<numRows) {
                        int c3=0;
                        while(c3<numCols) {
                            if (!(r3==rrr && c3==ccc)) {
                                bool mv2=isValidMove(rrr,ccc,r3,c3);
                                if (mv2) opScore+=0.1f;
                            }
                            c3++;
                        }
                        r3++;
                    }
                }
                ccc++;
            }
            rrr++;
        }
        return myScore - opScore;
    }

    float ChessBoard::getHighestNextScore()
    {
        bool orgEnP=enP;
        int orgEnPR=enPRow;
        int orgEnPC=enPCol;
        float bScore=-999999.0f;
        Color me=turn;
        Color oldTurn=turn;
        int rr=0;
        while(rr<numRows) {
            int cc=0;
            while(cc<numCols) {
                ChessPiece *pp=getPiece(rr,cc);
                if (pp!=nullptr && pp->getColor()==me) {
                    int r2=0;
                    while(r2<numRows) {
                        int c2=0;
                        while(c2<numCols) {
                            if (!(r2==rr && c2==cc)) {
                                bool mv=isValidMove(rr,cc,r2,c2);
                                if (mv) {
                                    ChessPiece *victim=getPiece(r2,c2);
                                    ChessPiece *capSave=victim;
                                    ChessPiece *orgP=pp;
                                    int orgRow=pp->getRow();
                                    int orgCol=pp->getColumn();
                                    bool saveEnP=enP;
                                    int saveEnPR=enPRow;
                                    int saveEnPC=enPCol;
                                    Color saveTurn=turn;

                                    bool doEnP=false;
                                    ChessPiece *enPCap=nullptr;
                                    if (pp->getType()==Pawn && enP) {
                                        if (r2==enPRow && c2==enPCol) {
                                            doEnP=true;
                                        }
                                    }

                                    if (!doEnP) {
                                        board[rr][cc]=nullptr;
                                        if (capSave!=nullptr) {
                                            delete capSave;
                                        }
                                        board[r2][c2]=pp;
                                        pp->setPosition(r2,c2);
                                    } else {
                                        int rp=(pp->getColor()==White?(enPRow+1):(enPRow-1));
                                        enPCap=getPiece(rp,enPCol);
                                        if(enPCap!=nullptr) delete enPCap;
                                        board[rp][enPCol]=nullptr;
                                        board[rr][cc]=nullptr;
                                        if (victim!=nullptr) delete victim;
                                        board[r2][c2]=pp;
                                        pp->setPosition(r2,c2);
                                    }

                                    if (turn==White) turn=Black; else turn=White;
                                    float sc=scoreBoard();
                                    if (sc>bScore) bScore=sc;
                                    if (turn==White) turn=Black; else turn=White;

                                    if (!doEnP) {
                                        board[rr][cc]=pp;
                                        board[r2][c2]=nullptr;
                                        pp->setPosition(orgRow,orgCol);
                                    } else {
                                        int rp=(pp->getColor()==White?(enPRow+1):(enPRow-1));
                                        board[rp][enPCol]=enPCap;
                                        board[rr][cc]=pp;
                                        board[r2][c2]=nullptr;
                                        pp->setPosition(orgRow,orgCol);
                                    }
                                    enP=saveEnP;
                                    enPRow=saveEnPR;
                                    enPCol=saveEnPC;
                                    turn=saveTurn;
                                }
                            }
                            c2++;
                        }
                        r2++;
                    }
                }
                cc++;
            }
            rr++;
        }
        if (bScore==-999999.0f) {
            float sc=scoreBoard();
            enP=orgEnP;
            enPRow=orgEnPR;
            enPCol=orgEnPC;
            turn=oldTurn;
            return sc;
        }
        enP=orgEnP;
        enPRow=orgEnPR;
        enPCol=orgEnPC;
        turn=oldTurn;
        return bScore;
    }
}
