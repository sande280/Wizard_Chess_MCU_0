#pragma once

#include <stdio.h>
#include <vector>
#include <utility>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"

#define LED_STRIP_GPIO_PIN  9
#define LED_STRIP_USE_DMA  1

// Numbers of the LED in the strip
#define LED_STRIP_LED_COUNT 96
#define LED_STRIP_MEMORY_BLOCK_WORDS 0 // let the driver choose a proper memory block size automatically
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

#define INVERT_X 1
#define INVERT_Y 1

// LED Color Constants
#define LED_GREEN_R 0
#define LED_GREEN_G 255
#define LED_GREEN_B 0

#define LED_WHITE_R 255
#define LED_WHITE_G 255
#define LED_WHITE_B 255

#define LED_BLUE_R 0
#define LED_BLUE_G 0
#define LED_BLUE_B 255

#define LED_OFF_R 0
#define LED_OFF_G 0
#define LED_OFF_B 0

#define LED_RED_R 255
#define LED_RED_G 0
#define LED_RED_B 0

// Forward declaration
namespace Student { class ChessBoard; }

class leds
{
private:

    led_strip_handle_t led_strip;

public:
    /**
     * Initializes the LED strip
     */
    void init();

    /**
     * Refreshes the LED strip (causes new colors to light up)
     */
    void refresh();

    /**
     * Updates the led color at position (x,y) to color (r,g,b)
     */
    void update_led(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b);

    /**
     * Light up LEDs for possible move destinations
     * @param moves Vector of (row, col) pairs in chess coordinates (0-7)
     */
    void showPossibleMoves(const std::vector<std::pair<int, int>>& moves);

    /**
     * Clear all LEDs on the main board area (rows 2-9)
     */
    void clearPossibleMoves();

    /**
     * Show board setup state - RED for occupied starting squares
     * @param reedGrid 12-byte array of reed switch states
     */
    void showBoardSetupState(uint8_t reedGrid[12]);

    /**
     * Show board ready state - all starting squares GREEN
     */
    void showBoardReady();

    /**
     * Show WHITE ambient lighting under all pieces on the board
     * @param board Reference to the chess board
     */
    void showAmbientWhite(Student::ChessBoard& board);

    /**
     * Show BLUE LED on the selected piece's source square
     * @param chessRow Chess row (0-7)
     * @param chessCol Chess column (0-7)
     */
    void showPieceSelected(int chessRow, int chessCol);

    /**
     * Show piece selected (BLUE) and possible moves (GREEN) together
     * Clears board first, then shows BLUE on source and GREEN on destinations
     * @param chessRow Source chess row (0-7)
     * @param chessCol Source chess column (0-7)
     * @param moves Vector of valid move destinations in chess coordinates
     */
    void showPiecePickup(int chessRow, int chessCol, const std::vector<std::pair<int, int>>& moves);

};

extern leds* led;