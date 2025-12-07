#include "leds.hpp"

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