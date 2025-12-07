#pragma once

#include <stdio.h>
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

#define INVERT_X 0
#define INVERT_Y 0

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

};

leds* led;