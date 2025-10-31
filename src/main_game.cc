#if 0

#include <iostream>
#include <string>
#include <sstream>
#include <limits>
#include <cstdlib>
#include "Chess.h"
#include "ChessBoard.hh"
#include "ChessPiece.hh"

using namespace Student;
using namespace std;

void clearScreen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

void printWelcome() {
    cout << "======================================\n";
    cout << "       WELCOME TO CHESS GAME         \n";
    cout << "======================================\n";
    cout << "Enter moves as: row1 col1 row2 col2\n";
    cout << "Example: 6 4 4 4 (moves piece from 6,4 to 4,4)\n";
    cout << "Type 'quit' to exit, 'help' for commands\n";
    cout << "======================================\n\n";
}

void printHelp() {
    cout << "\n=== CHESS COMMANDS ===\n";
    cout << "Move format: row1 col1 row2 col2\n";
    cout << "  Example: 6 4 4 4\n";
    cout << "Special commands:\n";
    cout << "  'quit'  - Exit the game\n";
    cout << "  'help'  - Show this help\n";
    cout << "  'board' - Redraw the board\n";
    cout << "  'new'   - Start a new game\n";
    cout << "  'ai'    - Let AI make a move (Black)\n";
    cout << "======================\n\n";
}

void setupStandardBoard(ChessBoard& board) {
    // Setup white pieces (bottom rows 6-7)
    // Pawns
    for (int col = 0; col < 8; col++) {
        board.createChessPiece(White, Pawn, 6, col);
    }

    // Back row
    board.createChessPiece(White, Rook, 7, 0);
    board.createChessPiece(White, Knight, 7, 1);
    board.createChessPiece(White, Bishop, 7, 2);
    board.createChessPiece(White, Queen, 7, 3);
    board.createChessPiece(White, King, 7, 4);
    board.createChessPiece(White, Bishop, 7, 5);
    board.createChessPiece(White, Knight, 7, 6);
    board.createChessPiece(White, Rook, 7, 7);

    // Setup black pieces (top rows 0-1)
    // Pawns
    for (int col = 0; col < 8; col++) {
        board.createChessPiece(Black, Pawn, 1, col);
    }

    // Back row
    board.createChessPiece(Black, Rook, 0, 0);
    board.createChessPiece(Black, Knight, 0, 1);
    board.createChessPiece(Black, Bishop, 0, 2);
    board.createChessPiece(Black, Queen, 0, 3);
    board.createChessPiece(Black, King, 0, 4);
    board.createChessPiece(Black, Bishop, 0, 5);
    board.createChessPiece(Black, Knight, 0, 6);
    board.createChessPiece(Black, Rook, 0, 7);
}

string colorToString(Color c) {
    return (c == White) ? "White" : "Black";
}

bool isKingInCheck(ChessBoard& board, Color color) {
    // Find the king
    int kingRow = -1, kingCol = -1;
    for (int r = 0; r < board.getNumRows(); r++) {
        for (int c = 0; c < board.getNumCols(); c++) {
            ChessPiece* piece = board.getPiece(r, c);
            if (piece && piece->getType() == King && piece->getColor() == color) {
                kingRow = r;
                kingCol = c;
                break;
            }
        }
        if (kingRow != -1) break;
    }

    if (kingRow == -1) return false; // No king found

    // Check if king is under attack
    // isPieceUnderThreat checks if any enemy piece can attack this position
    return board.isPieceUnderThreat(kingRow, kingCol);
}

bool hasValidMoves(ChessBoard& board, Color color) {
    for (int fromRow = 0; fromRow < board.getNumRows(); fromRow++) {
        for (int fromCol = 0; fromCol < board.getNumCols(); fromCol++) {
            ChessPiece* piece = board.getPiece(fromRow, fromCol);
            if (!piece || piece->getColor() != color) continue;

            for (int toRow = 0; toRow < board.getNumRows(); toRow++) {
                for (int toCol = 0; toCol < board.getNumCols(); toCol++) {
                    if (board.isValidMove(fromRow, fromCol, toRow, toCol)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void makeAIMove(ChessBoard& board) {
    cout << "AI is thinking...\n";

    int bestFromRow = -1, bestFromCol = -1, bestToRow = -1, bestToCol = -1;
    int bestScore = -999999;

    // Try all possible moves for Black
    for (int fromRow = 0; fromRow < board.getNumRows(); fromRow++) {
        for (int fromCol = 0; fromCol < board.getNumCols(); fromCol++) {
            ChessPiece* piece = board.getPiece(fromRow, fromCol);
            if (!piece || piece->getColor() != Black) continue;

            for (int toRow = 0; toRow < board.getNumRows(); toRow++) {
                for (int toCol = 0; toCol < board.getNumCols(); toCol++) {
                    if (board.isValidMove(fromRow, fromCol, toRow, toCol)) {
                        // Evaluate this move
                        int score = 0;

                        // Bonus for capturing pieces
                        ChessPiece* target = board.getPiece(toRow, toCol);
                        if (target) {
                            switch(target->getType()) {
                                case Pawn: score += 10; break;
                                case Knight: score += 30; break;
                                case Bishop: score += 30; break;
                                case Rook: score += 50; break;
                                case Queen: score += 90; break;
                                case King: score += 900; break;
                            }
                        }

                        // Bonus for center control
                        if (toRow >= 3 && toRow <= 4 && toCol >= 3 && toCol <= 4) {
                            score += 5;
                        }

                        // Bonus for pawn advancement
                        if (piece->getType() == Pawn) {
                            score += toRow; // Black pawns move down
                        }

                        if (score > bestScore) {
                            bestScore = score;
                            bestFromRow = fromRow;
                            bestFromCol = fromCol;
                            bestToRow = toRow;
                            bestToCol = toCol;
                        }
                    }
                }
            }
        }
    }

    if (bestFromRow != -1) {
        cout << "AI moves from (" << bestFromRow << "," << bestFromCol
             << ") to (" << bestToRow << "," << bestToCol << ")\n";
        board.movePiece(bestFromRow, bestFromCol, bestToRow, bestToCol);
    } else {
        cout << "AI has no valid moves!\n";
    }
}

int main() {
    ChessBoard board(8, 8);
    setupStandardBoard(board);

    Color currentPlayer = White;
    bool gameRunning = true;
    string input;

    clearScreen();
    printWelcome();
    cout << board.displayBoard().str() << endl;

    while (gameRunning) {
        // Check for check/checkmate
        bool inCheck = isKingInCheck(board, currentPlayer);
        bool hasValidMove = hasValidMoves(board, currentPlayer);

        if (inCheck && !hasValidMove) {
            cout << "\nCHECKMATE! " << colorToString(currentPlayer == White ? Black : White) << " wins!\n";
            cout << "Type 'new' for a new game or 'quit' to exit: ";
            getline(cin, input);
            if (input == "new") {
                board = ChessBoard(8, 8);
                setupStandardBoard(board);
                currentPlayer = White;
                clearScreen();
                printWelcome();
                cout << board.displayBoard().str() << endl;
                continue;
            } else {
                break;
            }
        } else if (!hasValidMove) {
            cout << "\nSTALEMATE! It's a draw!\n";
            cout << "Type 'new' for a new game or 'quit' to exit: ";
            getline(cin, input);
            if (input == "new") {
                board = ChessBoard(8, 8);
                setupStandardBoard(board);
                currentPlayer = White;
                clearScreen();
                printWelcome();
                cout << board.displayBoard().str() << endl;
                continue;
            } else {
                break;
            }
        } else if (inCheck) {
            cout << "\n*** CHECK! ***\n";
        }

        cout << "\n" << colorToString(currentPlayer) << "'s turn.\n";
        cout << "Enter move (or 'help' for commands): ";
        getline(cin, input);

        if (input == "quit") {
            cout << "Thanks for playing!\n";
            break;
        } else if (input == "help") {
            printHelp();
            continue;
        } else if (input == "board") {
            clearScreen();
            cout << board.displayBoard().str() << endl;
            continue;
        } else if (input == "new") {
            board = ChessBoard(8, 8);
            setupStandardBoard(board);
            currentPlayer = White;
            clearScreen();
            printWelcome();
            cout << board.displayBoard().str() << endl;
            continue;
        } else if (input == "ai" && currentPlayer == Black) {
            makeAIMove(board);
            currentPlayer = (currentPlayer == White) ? Black : White;
            cout << board.displayBoard().str() << endl;
            continue;
        }

        // Parse move input
        istringstream iss(input);
        int fromRow, fromCol, toRow, toCol;

        if (!(iss >> fromRow >> fromCol >> toRow >> toCol)) {
            cout << "Invalid input format. Use: row1 col1 row2 col2\n";
            continue;
        }

        // Validate piece belongs to current player
        ChessPiece* piece = board.getPiece(fromRow, fromCol);
        if (!piece) {
            cout << "No piece at position (" << fromRow << "," << fromCol << ")\n";
            continue;
        }

        if (piece->getColor() != currentPlayer) {
            cout << "That's not your piece!\n";
            continue;
        }

        // Try to make the move
        if (board.isValidMove(fromRow, fromCol, toRow, toCol)) {
            board.movePiece(fromRow, fromCol, toRow, toCol);
            currentPlayer = (currentPlayer == White) ? Black : White;
            clearScreen();
            cout << board.displayBoard().str() << endl;

            // Optional: Auto-play AI for Black
            if (currentPlayer == Black) {
                makeAIMove(board);
                currentPlayer = White;
                cout << board.displayBoard().str() << endl;
            }
        } else {
            cout << "Invalid move! Try again.\n";
        }
    }

    return 0;
}

#endif