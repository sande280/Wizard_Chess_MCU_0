#pragma once

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

#include "leds.hpp"

#define I2C_SCL             GPIO_NUM_6       
#define I2C_SDA             GPIO_NUM_5
#define I2C_NUM             I2C_NUM_0
#define I2C_FREQ_HZ         400000
#define I2C_TX_BUF_DISABLE  0
#define I2C_RX_BUF_DISABLE  0
#define I2C_TIMEOUT_MS      1000

#define PCAL6524_RESET_PIN          GPIO_NUM_7       /*!< GPIO for hardware reset */
#define PCAL6524_ADDR               0x22    /*!< I2C address (ADDR tied to GND) */

/* --- Register Definitions --- */
#define REG_INPUT_P0        0x00
#define REG_INPUT_P1        0x01
#define REG_INPUT_P2        0x02
#define REG_OUTPUT_P0       0x04
#define REG_OUTPUT_P1       0x05
#define REG_OUTPUT_P2       0x06
#define REG_CONFIG_P0       0x0C
#define REG_CONFIG_P1       0x0D
#define REG_CONFIG_P2       0x0E
#define REG_PUPD_EN_P0      0x4C
#define REG_PUPD_EN_P1      0x4D
#define REG_PUPD_EN_P2      0x4E
#define REG_PUPD_SEL_P0     0x50
#define REG_PUPD_SEL_P1     0x51
#define REG_PUPD_SEL_P2     0x52

class reed
{
private:
    void init_i2c();
    void init_gpio();
    void init_pcal();


    void write_reg(uint8_t reg, uint8_t val);
    uint8_t read_reg(uint8_t reg);

    static void scan_task_trampoline(void* arg) {
        reinterpret_cast<reed*>(arg)->scan_task();
    }
    void scan_task();

    TaskHandle_t scan_task_handle = NULL;

public:
    void init();
    void read_matrix();
    void start_scan_task();
    bool wait_for_col(uint8_t row, uint8_t col);
    uint8_t grid[12];
};

reed* switches;