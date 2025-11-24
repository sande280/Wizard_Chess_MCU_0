

 #if 0

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"


typedef enum {
    EMPTY = 0,
    PAWN,
    ROOK,
    KNIGHT,
    BISHOP,
    QUEEN,
    KING
} PieceType;


typedef enum {
    NONE = 0,
    WHITE,
    BLACK
} Color;


typedef struct {
    PieceType type;
    Color color;
} ChessPiece;


ChessPiece board[8][8];
Color currentTurn = WHITE;


void initBoard() {

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            board[i][j].type = EMPTY;
            board[i][j].color = NONE;
        }
    }



    board[0][0] = (ChessPiece){ROOK, BLACK};
    board[0][1] = (ChessPiece){KNIGHT, BLACK};
    board[0][2] = (ChessPiece){BISHOP, BLACK};
    board[0][3] = (ChessPiece){QUEEN, BLACK};
    board[0][4] = (ChessPiece){KING, BLACK};
    board[0][5] = (ChessPiece){BISHOP, BLACK};
    board[0][6] = (ChessPiece){KNIGHT, BLACK};
    board[0][7] = (ChessPiece){ROOK, BLACK};


    for (int j = 0; j < 8; j++) {
        board[1][j] = (ChessPiece){PAWN, BLACK};
    }



    for (int j = 0; j < 8; j++) {
        board[6][j] = (ChessPiece){PAWN, WHITE};
    }


    board[7][0] = (ChessPiece){ROOK, WHITE};
    board[7][1] = (ChessPiece){KNIGHT, WHITE};
    board[7][2] = (ChessPiece){BISHOP, WHITE};
    board[7][3] = (ChessPiece){QUEEN, WHITE};
    board[7][4] = (ChessPiece){KING, WHITE};
    board[7][5] = (ChessPiece){BISHOP, WHITE};
    board[7][6] = (ChessPiece){KNIGHT, WHITE};
    board[7][7] = (ChessPiece){ROOK, WHITE};
}


char getPieceChar(ChessPiece piece) {
    char c = ' ';
    switch (piece.type) {
        case PAWN:   c = 'p'; break;
        case ROOK:   c = 'r'; break;
        case KNIGHT: c = 'n'; break;
        case BISHOP: c = 'b'; break;
        case QUEEN:  c = 'q'; break;
        case KING:   c = 'k'; break;
        default:     return ' ';
    }


    if (piece.color == WHITE) {
        c = toupper(c);
    }

    return c;
}


void displayBoard() {
    printf("\n  01234567\n");
    printf("  --------\n");
    for (int i = 0; i < 8; i++) {
        printf("%d|", i);
        for (int j = 0; j < 8; j++) {
            printf("%c", getPieceChar(board[i][j]));
        }
        printf("|\n");
    }
    printf("  --------\n\n");
}


int isValidPosition(int row, int col) {
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}


int isPathClear(int r1, int c1, int r2, int c2) {
    int dr = (r2 > r1) ? 1 : ((r2 < r1) ? -1 : 0);
    int dc = (c2 > c1) ? 1 : ((c2 < c1) ? -1 : 0);

    int r = r1 + dr;
    int c = c1 + dc;

    while (r != r2 || c != c2) {
        if (board[r][c].type != EMPTY) {
            return 0;
        }
        r += dr;
        c += dc;
    }

    return 1;
}


int isValidPawnMove(int r1, int c1, int r2, int c2, Color color) {
    int direction = (color == WHITE) ? -1 : 1;
    int startRow = (color == WHITE) ? 6 : 1;


    if (c1 == c2) {

        if (r2 == r1 + direction && board[r2][c2].type == EMPTY) {
            return 1;
        }

        if (r1 == startRow && r2 == r1 + 2 * direction &&
            board[r1 + direction][c1].type == EMPTY &&
            board[r2][c2].type == EMPTY) {
            return 1;
        }
    }

    else if (abs(c2 - c1) == 1 && r2 == r1 + direction) {
        if (board[r2][c2].type != EMPTY && board[r2][c2].color != color) {
            return 1;
        }
    }

    return 0;
}


int isValidRookMove(int r1, int c1, int r2, int c2) {
    if (r1 != r2 && c1 != c2) {
        return 0;
    }
    return isPathClear(r1, c1, r2, c2);
}


int isValidBishopMove(int r1, int c1, int r2, int c2) {
    if (abs(r2 - r1) != abs(c2 - c1)) {
        return 0;
    }
    return isPathClear(r1, c1, r2, c2);
}


int isValidKnightMove(int r1, int c1, int r2, int c2) {
    int dr = abs(r2 - r1);
    int dc = abs(c2 - c1);
    return (dr == 2 && dc == 1) || (dr == 1 && dc == 2);
}


int isValidQueenMove(int r1, int c1, int r2, int c2) {
    return isValidRookMove(r1, c1, r2, c2) || isValidBishopMove(r1, c1, r2, c2);
}


int isValidKingMove(int r1, int c1, int r2, int c2) {
    return abs(r2 - r1) <= 1 && abs(c2 - c1) <= 1;
}


int isValidMove(int r1, int c1, int r2, int c2) {

    if (!isValidPosition(r1, c1) || !isValidPosition(r2, c2)) {
        printf("Position out of bounds!\n");
        return 0;
    }


    if (r1 == r2 && c1 == c2) {
        printf("Can't move to same position!\n");
        return 0;
    }

    ChessPiece piece = board[r1][c1];
    ChessPiece target = board[r2][c2];


    if (piece.type == EMPTY) {
        printf("No piece at source position!\n");
        return 0;
    }


    if (piece.color != currentTurn) {
        printf("It's %s's turn!\n", currentTurn == WHITE ? "White" : "Black");
        return 0;
    }


    if (target.type != EMPTY && target.color == piece.color) {
        printf("Can't capture your own piece!\n");
        return 0;
    }


    int valid = 0;
    switch (piece.type) {
        case PAWN:
            valid = isValidPawnMove(r1, c1, r2, c2, piece.color);
            break;
        case ROOK:
            valid = isValidRookMove(r1, c1, r2, c2);
            break;
        case KNIGHT:
            valid = isValidKnightMove(r1, c1, r2, c2);
            break;
        case BISHOP:
            valid = isValidBishopMove(r1, c1, r2, c2);
            break;
        case QUEEN:
            valid = isValidQueenMove(r1, c1, r2, c2);
            break;
        case KING:
            valid = isValidKingMove(r1, c1, r2, c2);
            break;
        default:
            valid = 0;
    }

    if (!valid) {
        printf("Invalid move for %s!\n",
               piece.type == PAWN ? "pawn" :
               piece.type == ROOK ? "rook" :
               piece.type == KNIGHT ? "knight" :
               piece.type == BISHOP ? "bishop" :
               piece.type == QUEEN ? "queen" :
               piece.type == KING ? "king" : "piece");
    }

    return valid;
}


void makeMove(int r1, int c1, int r2, int c2) {
    board[r2][c2] = board[r1][c1];
    board[r1][c1].type = EMPTY;
    board[r1][c1].color = NONE;


    currentTurn = (currentTurn == WHITE) ? BLACK : WHITE;
}


int readLine(char *buffer, int maxLen) {
    int pos = 0;
    while (pos < maxLen - 1) {
        int c = getchar();
        if (c == EOF) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }

        if (c == '\n' || c == '\r') {
            if (pos > 0) {
                buffer[pos] = '\0';
                printf("\n");
                return pos;
            }
        } else if (c == 127 || c == '\b') {
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (c >= 32 && c < 127) {
            buffer[pos++] = c;
            putchar(c);
            fflush(stdout);
        }
    }
    buffer[pos] = '\0';
    return pos;
}


void chessGame() {
    char input[100];
    int r1, c1, r2, c2;

    printf("\n======================================\n");
    printf("       WELCOME TO ESP32 CHESS\n");
    printf("======================================\n");
    printf("Enter moves as: row1 col1 row2 col2\n");
    printf("Example: 6 4 4 4 (moves pawn from 6,4 to 4,4)\n");
    printf("Commands: quit, help, board, new\n");
    printf("======================================\n");

    initBoard();
    displayBoard();

    while (1) {
        printf("\n%s's turn.\n", currentTurn == WHITE ? "White" : "Black");
        printf("Enter move: ");
        fflush(stdout);

        memset(input, 0, sizeof(input));
        int len = readLine(input, sizeof(input));

        if (len == 0) {
            continue;
        }


        if (strcmp(input, "quit") == 0) {
            printf("Thanks for playing!\n");
            break;
        } else if (strcmp(input, "board") == 0) {
            displayBoard();
            continue;
        } else if (strcmp(input, "new") == 0) {
            initBoard();
            currentTurn = WHITE;
            printf("New game started!\n");
            displayBoard();
            continue;
        } else if (strcmp(input, "help") == 0) {
            printf("\nHow to play:\n");
            printf("- Enter moves as: row1 col1 row2 col2\n");
            printf("- Rows and columns are numbered 0-7\n");
            printf("- White pieces are UPPERCASE, black are lowercase\n");
            printf("- Commands:\n");
            printf("  board - Show the board\n");
            printf("  new   - Start a new game\n");
            printf("  quit  - Exit the game\n\n");
            continue;
        }


        if (sscanf(input, "%d %d %d %d", &r1, &c1, &r2, &c2) != 4) {
            printf("Invalid input format! Use: row1 col1 row2 col2\n");
            continue;
        }


        if (isValidMove(r1, c1, r2, c2)) {
            printf("Moving piece from (%d,%d) to (%d,%d)\n", r1, c1, r2, c2);
            makeMove(r1, c1, r2, c2);
            displayBoard();
        } else {
            printf("Invalid move!\n");
        }
    }
}

void app_main(void) {

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    chessGame();


    printf("Restarting in 5 seconds...\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    esp_restart();
}

#endif