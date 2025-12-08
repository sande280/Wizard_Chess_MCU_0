#include "leds.hpp"
#include "ChessBoard.hh"

// Define TAG for logging
static const char* TAG = "LEDS";


void leds::init()
{
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_PIN, // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_COUNT,      // The number of LEDs in the strip,
        .led_model = LED_MODEL_WS2812,        
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        }
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .flags = {
            .with_dma = LED_STRIP_USE_DMA,
        }
    };

    // LED Strip object handle
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
}

void leds::refresh()
{
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}

void leds::update_led(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b)
{
#if INVERT_X
    x = 11 - x;
#endif
#if INVERT_Y
    y = 7 - y;
#endif

    uint8_t index = (y % 2 ? 11 - x : x) + 12 * y;
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, index, r, g, b));
}

void leds::showPossibleMoves(const std::vector<std::pair<int, int>>& moves)
{
    // Clear LEDs first
    clearPossibleMoves();

    // Light up each possible destination in green
    for (const auto& move : moves) {
        int chessRow = move.first;
        int chessCol = move.second;
        // Map chess coords (0-7) to physical LED coords
        // Physical row = chess row + 2 (rows 0-1 are capture zones)
        int physRow = 9 - chessCol;
        int physCol = chessRow;
        update_led(physRow, physCol, 0, 255, 0);  // Green for valid moves
    }
    refresh();
}

void leds::clearPossibleMoves()
{
    // Turn off LEDs for main board area (physical rows 2-9 = chess rows 0-7)
    for (int physRow = 2; physRow < 10; physRow++) {
        for (int col = 0; col < 8; col++) {
            update_led(physRow, col, LED_OFF_R, LED_OFF_G, LED_OFF_B);
        }
    }
    refresh();
}

void leds::showBoardSetupState(uint8_t reedGrid[12])
{
    // Show GREEN for occupied starting squares, RED for missing pieces
    // Starting positions: chess rows 0-1 (black) and 6-7 (white)
    // Mapping: physRow = 9 - chessCol, physCol = chessRow
    // So starting squares are physCol 0-1 (black) and physCol 6-7 (white)

    // First clear the main board area
    for (int physRow = 2; physRow < 10; physRow++) {
        for (int col = 0; col < 8; col++) {
            update_led(physRow, col, LED_OFF_R, LED_OFF_G, LED_OFF_B);
        }
    }

    // Check starting positions only (physCol 0-1 for black, 6-7 for white)
    for (int physRow = 2; physRow < 10; physRow++) {
        uint8_t rowData = reedGrid[physRow];

        // Black starting positions (chess rows 0-1 = physCol 0-1)
        for (int col = 0; col < 2; col++) {
            if (rowData & (1 << col)) {
                // Piece present - GREEN (ready)
                update_led(physRow, col, LED_GREEN_R, LED_GREEN_G, LED_GREEN_B);
            } else {
                // Piece missing - RED (needs attention)
                update_led(physRow, col, LED_RED_R, LED_RED_G, LED_RED_B);
            }
        }

        // White starting positions (chess rows 6-7 = physCol 6-7)
        for (int col = 6; col < 8; col++) {
            if (rowData & (1 << col)) {
                // Piece present - GREEN (ready)
                update_led(physRow, col, LED_GREEN_R, LED_GREEN_G, LED_GREEN_B);
            } else {
                // Piece missing - RED (needs attention)
                update_led(physRow, col, LED_RED_R, LED_RED_G, LED_RED_B);
            }
        }
    }
    refresh();
}

void leds::showBoardReady()
{
    // Show GREEN on all starting positions to indicate board is ready
    // Mapping: physRow = 9 - chessCol, physCol = chessRow
    // Black (chess rows 0-1) → physCol 0-1
    // White (chess rows 6-7) → physCol 6-7

    // First clear the main board area
    for (int physRow = 2; physRow < 10; physRow++) {
        for (int col = 0; col < 8; col++) {
            update_led(physRow, col, LED_OFF_R, LED_OFF_G, LED_OFF_B);
        }
    }

    // Light up GREEN on starting columns (physCol 0-1 for black, 6-7 for white)
    for (int physRow = 2; physRow < 10; physRow++) {
        // Black starting positions (chess rows 0-1 = physCol 0-1)
        update_led(physRow, 0, LED_GREEN_R, LED_GREEN_G, LED_GREEN_B);
        update_led(physRow, 1, LED_GREEN_R, LED_GREEN_G, LED_GREEN_B);
        // White starting positions (chess rows 6-7 = physCol 6-7)
        update_led(physRow, 6, LED_GREEN_R, LED_GREEN_G, LED_GREEN_B);
        update_led(physRow, 7, LED_GREEN_R, LED_GREEN_G, LED_GREEN_B);
    }
    refresh();
}

void leds::showAmbientWhite(Student::ChessBoard& board)
{
    // First clear the main board area
    for (int physRow = 2; physRow < 10; physRow++) {
        for (int col = 0; col < 8; col++) {
            update_led(physRow, col, LED_OFF_R, LED_OFF_G, LED_OFF_B);
        }
    }

    // Light up WHITE under each piece on the board
    for (int chessRow = 0; chessRow < 8; chessRow++) {
        for (int chessCol = 0; chessCol < 8; chessCol++) {
            if (board.getPiece(chessRow, chessCol) != nullptr) {
                // Convert chess coords to physical coords
                int physRow = 9 - chessCol;
                int physCol = chessRow;
                update_led(physRow, physCol, LED_WHITE_R, LED_WHITE_G, LED_WHITE_B);
            }
        }
    }
    refresh();
}

void leds::showPieceSelected(int chessRow, int chessCol)
{
    // Convert chess coords to physical coords
    int physRow = 9 - chessCol;
    int physCol = chessRow;
    update_led(physRow, physCol, LED_BLUE_R, LED_BLUE_G, LED_BLUE_B);
    refresh();
}

void leds::showPiecePickup(int chessRow, int chessCol, const std::vector<std::pair<int, int>>& moves)
{
    // Clear the board first
    for (int physRow = 2; physRow < 10; physRow++) {
        for (int col = 0; col < 8; col++) {
            update_led(physRow, col, LED_OFF_R, LED_OFF_G, LED_OFF_B);
        }
    }

    // Show BLUE on the source square
    int srcPhysRow = 9 - chessCol;
    int srcPhysCol = chessRow;
    update_led(srcPhysRow, srcPhysCol, LED_BLUE_R, LED_BLUE_G, LED_BLUE_B);

    // Show GREEN on valid move destinations
    for (const auto& move : moves) {
        int destChessRow = move.first;
        int destChessCol = move.second;
        int destPhysRow = 9 - destChessCol;
        int destPhysCol = destChessRow;
        update_led(destPhysRow, destPhysCol, LED_GREEN_R, LED_GREEN_G, LED_GREEN_B);
    }
    refresh();
}