// Add these includes at the top of chess_ui.c (if not already present)
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Replace the existing update_board() function with this:

void update_board() {
    const char *pieces[] = {
        "", "♟", "♝", "♞", "♜", "♛", "♚", "♙", "♗", "♘", "♖", "♕", "♔"
    };

    uint8_t new_board[33] = {0};
    int max_retries = 30;
    bool valid_data = false;

    // Retry loop - ESP32 needs time to process move + AI thinking
    for (int retry = 0; retry < max_retries; retry++) {
        i2c_comm_read(0x67, new_board, sizeof(new_board));

        if (new_board[0] == 0xAA) {
            valid_data = true;
            break;
        }

        printf("Waiting for board state (retry %d, got 0x%02X)...\n", retry + 1, new_board[0]);
        vTaskDelay(pdMS_TO_TICKS(200));  // Wait 200ms between retries
    }

    if (!valid_data) {
        printf("Failed to get valid board state after %d retries\n", max_retries);
        for(int i = 0; i < 33; i++) {
            printf("%02X", new_board[i]);
        }
        printf("\n");
        return;
    }

    // Decode nibbles into 64 squares
    uint8_t squares[64];
    for(int i = 0; i < 32; i++) {
        uint8_t b = new_board[i + 1];
        squares[2*i] = (b >> 4) & 0x0F;
        squares[2*i + 1] = b & 0x0F;
    }

    // Update UI and print debug
    printf("Board state received:\n");
    for (int i = 0; i < BOARD_W; i++) {
        for (int j = 0; j < BOARD_W; j++) {
            lv_label_set_text(board_piece[j][i], pieces[squares[8*i+j]]);
            if(strcmp(pieces[squares[8*i+j]], "") == 0) {
                printf("  ");
            }
            else {
                printf("%s ", pieces[squares[8*i+j]]);
            }
        }
        printf("\n");
    }
}
