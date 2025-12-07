#include "reed.hpp"
#include "PinDefs.h"

// Define TAG for logging
static const char* TAG = "REED";

void reed::init()
{
    init_gpio();
    init_i2c();
    init_pcal();
}

void reed::init_i2c()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
    };

    conf.master.clk_speed = I2C_FREQ_HZ;

    i2c_param_config(I2C_NUM, &conf);

    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM, conf.mode, I2C_RX_BUF_DISABLE, I2C_TX_BUF_DISABLE, 0));
}

void reed::init_gpio()
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PCAL6524_RESET_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    //Pulse Reset Pin
    gpio_set_level(PCAL6524_RESET_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PCAL6524_RESET_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void reed::init_pcal()
{
    ESP_LOGI(TAG, "Initializing PCAL");

    //Config
    write_reg(REG_CONFIG_P0, 0xFF);
    write_reg(REG_CONFIG_P1, 0x00);
    write_reg(REG_CONFIG_P2, 0x00);

    write_reg(REG_PUPD_EN_P0, 0xFF);

    write_reg(REG_PUPD_SEL_P0, 0x00);

    write_reg(REG_OUTPUT_P1, 0x00);
    write_reg(REG_OUTPUT_P2, 0x00);

    ESP_LOGI(TAG, "PCAL Configured");
}

esp_err_t reed::write_reg(uint8_t reg, uint8_t val)
{
    uint8_t write_buf[2] = {reg, val};
    esp_err_t ret = i2c_master_write_to_device(I2C_NUM, PCAL6524_ADDR, write_buf, sizeof(write_buf), I2C_TIMEOUT_MS / portTICK_PERIOD_MS);

    if(ret != ESP_OK)
    {
        ESP_LOGI(TAG, "Write failed to PCAL6524");
        return ESP_FAIL;
    }
    return ESP_OK;
}

uint8_t reed::read_reg(uint8_t reg)
{
    uint8_t data = 0;
    esp_err_t ret = i2c_master_write_read_device(I2C_NUM, PCAL6524_ADDR, &reg, 1, &data, 1, I2C_TIMEOUT_MS / portTICK_PERIOD_MS);

    if (ret != ESP_OK) {
        // Return 0 so logic sees "no buttons pressed"
        return 0; 
    }
    return data;
}

void reed::read_matrix()
{
    for(int row = 0; row < 8; row++)
    {
        uint8_t row_mask = 1 << row;
        write_reg(REG_OUTPUT_P1, row_mask);
        vTaskDelay(pdMS_TO_TICKS(5));

        uint8_t col_data = read_reg(REG_INPUT_P0);

        col_data = (col_data & 0xF0) >> 4 | (col_data & 0x0F) << 4;
        col_data = (col_data & 0xCC) >> 2 | (col_data & 0x33) << 2;
        col_data = (col_data & 0xAA) >> 1 | (col_data & 0x55) << 1;

        grid[row] = col_data;

        write_reg(REG_OUTPUT_P1, 0x00);
    }

    for(int row = 0; row < 4; row++)
    {
        uint8_t row_mask = 1 << row;
        write_reg(REG_OUTPUT_P2, row_mask);
        vTaskDelay(pdMS_TO_TICKS(5));

        uint8_t col_data = read_reg(REG_INPUT_P0);

        col_data = (col_data & 0xF0) >> 4 | (col_data & 0x0F) << 4;
        col_data = (col_data & 0xCC) >> 2 | (col_data & 0x33) << 2;
        col_data = (col_data & 0xAA) >> 1 | (col_data & 0x55) << 1;

        grid[row+8] = col_data;

        write_reg(REG_OUTPUT_P2, 0x00);
    }
}

void reed::start_scan_task()
{
    xTaskCreate(scan_task_trampoline, "reed_scan_task", 4096, this, 5, &scan_task_handle);
}

void reed::scan_task()
{
    while (1)
    {
        bool mag = gpio_get_level(MAGNET_PIN);
        if (gantry.position_reached == false && !mag) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        read_matrix();
        ESP_LOGI(TAG, "Reed Matrix:");
        for (int i = 0; i < 12; i++)
        {
            //ESP_LOGI(TAG, "Row %d: 0x%02X", i, grid[i]);
        }

        //Junk Code
        if(grid[2] & 0x01)
        {
            led->update_led(2,0,255,0,0);
        }
        else
        {
            led->update_led(2,0,0,255,0);
        }
        led->refresh();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

bool reed::wait_for_col(uint8_t row, uint8_t col)
{
    while (1)
    {
        uint8_t col_data = 0;
        if (row < 8)
        {
            uint8_t row_mask = 1 << row;
            write_reg(REG_OUTPUT_P1, row_mask);
            vTaskDelay(pdMS_TO_TICKS(5));
            col_data = read_reg(REG_INPUT_P0);
            write_reg(REG_OUTPUT_P1, 0x00);
        }
        else
        {
            uint8_t row_mask = 1 << (row - 8);
            write_reg(REG_OUTPUT_P2, row_mask);
            vTaskDelay(pdMS_TO_TICKS(5));
            col_data = read_reg(REG_INPUT_P0);
            write_reg(REG_OUTPUT_P2, 0x00);
        }

        if (col_data & (1 << col))
        {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool reed::isPopulated(uint8_t row, uint8_t col)
{
    int i = 0;
    while (i < 10)
    {
        read_matrix();
        /*
        uint8_t col_data = 0;
        if (row < 8)
        {
            uint8_t row_mask = 1 << row;
            write_reg(REG_OUTPUT_P1, row_mask);
            vTaskDelay(pdMS_TO_TICKS(5));
            col_data = read_reg(REG_INPUT_P0);
            write_reg(REG_OUTPUT_P1, 0x00);
        }
        else
        {
            uint8_t row_mask = 1 << (row - 8);
            write_reg(REG_OUTPUT_P2, row_mask);
            vTaskDelay(pdMS_TO_TICKS(5));
            col_data = read_reg(REG_INPUT_P0);
            write_reg(REG_OUTPUT_P2, 0x00);
        }
        */
        if (grid[row] & (1 << col))
        {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        i++;
    }
    return false;
}
