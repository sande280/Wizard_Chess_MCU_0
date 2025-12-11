#include "audio.hpp"

// Define TAG for logging
static const char* TAG = "AUDIO";

void audio::init_gpio()
{
    // Setup GPIO3 ()
    gpio_reset_pin(GPIO_NUM_3);
    gpio_set_direction(GPIO_NUM_3, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_3, 0);

    // Setup GPIO46
    gpio_reset_pin(GPIO_NUM_46);
    gpio_set_direction(GPIO_NUM_46, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_46, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(GPIO_NUM_46, 1);
    ESP_LOGI(TAG, "Initialized GPIOs");
}

void audio::init_i2s()
{
    //Initialize handles to NULL
    tx_handle = NULL;
    rx_handle = NULL;

    // Initialize the I2S channels
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle)); // Assign to global handles

    // Configure the I2S channel
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_BITS_PER_SAMPLE, I2S_CHANNEL_FORMAT),
        .gpio_cfg = {
            .mclk = I2S_MCK_IO,
            .bclk = I2S_BCLK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_DOUT_IO,
            .din = I2S_DIN_IO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    // Initialize the TX/RX channels
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));

    // Enable the I2S channels
    ESP_LOGI(TAG, "Enabling I2S channels...");
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    
}

void audio::init()
{
    init_gpio();
    init_i2s();
}

void audio::play_sound(int32_t* audio_file, uint32_t audio_size)
{
    size_t bytes_written = 0;

    esp_err_t ret = i2s_channel_write(tx_handle, audio_file, audio_size * sizeof(int32_t), &bytes_written, portMAX_DELAY);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S Write Error: %s", esp_err_to_name(ret));
    } else if (bytes_written < audio_size * sizeof(int32_t)) {
        ESP_LOGW(TAG, "I2S Write Timeout: wrote %d bytes", bytes_written);
    }
}

void audio::set_mute(bool mute)
{
    if(mute)
    {
        gpio_set_level(GPIO_NUM_3, 1);
    }
    else
    {
        gpio_set_level(GPIO_NUM_3, 0);
    }
}

void audio::playback_task(void* pvParameters)
{
    audio* instance = (audio*)pvParameters;
    ESP_LOGI(TAG, "Starting playback task.");

    while(1)
    {
        // Prioritize one-shot sounds
        if (instance->m_play_oneshot_flag)
        {
            instance->play_sound(instance->m_oneshot_audio_buffer, instance->m_oneshot_buffer_size);
            // Clear the flag after playing
            instance->m_play_oneshot_flag = false;
        }
        // Otherwise, play the continuous sound
        else if (instance->m_continuous_audio_buffer != NULL && instance->m_continuous_buffer_size > 0)
        {
            instance->play_sound(instance->m_continuous_audio_buffer, instance->m_continuous_buffer_size);
        }
        else
        {
            // If no buffers are set, just yield for a moment.
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void audio::start_continuous_playback(int32_t* audio_buffer, uint32_t buffer_size)
{
    if (m_playback_task_handle != NULL) {
        ESP_LOGW(TAG, "Playback task is already running.");
        // If task is running, just update the buffer it should use.
        m_continuous_audio_buffer = audio_buffer;
        m_continuous_buffer_size = buffer_size;
        return;
    }

    m_continuous_audio_buffer = audio_buffer;
    m_continuous_buffer_size = buffer_size;

    xTaskCreate(
        playback_task,
        "AudioPlaybackTask",
        4096,
        this, // Pass the current object instance to the task
        5,
        &m_playback_task_handle
    );
}

void audio::stop_continuous_playback()
{
    if (m_playback_task_handle != NULL)
    {
        vTaskDelete(m_playback_task_handle);
        m_playback_task_handle = NULL;
        ESP_LOGI(TAG, "Stopped continuous playback task.");
    }

    // Clear buffer info
    m_continuous_audio_buffer = NULL;
    m_continuous_buffer_size = 0;
}

void audio::play_oneshot(int32_t* audio_buffer, uint32_t buffer_size)
{
    // Check if a one-shot is already pending
    if (m_play_oneshot_flag)
    {
        ESP_LOGW(TAG, "A one-shot sound is already pending, ignoring new request.");
        return;
    }

    // Set the buffer details
    m_oneshot_audio_buffer = audio_buffer;
    m_oneshot_buffer_size = buffer_size;

    // Set the flag to trigger playback in the task
    m_play_oneshot_flag = true;
}

void audio::play_tone(uint32_t frequency, uint32_t duration_ms, float volume)
{
    int32_t continuous_audio_file[I2S_SAMPLE_RATE * duration_ms * 2 / 1000] = {0};
    uint32_t continuous_buffer_size = I2S_SAMPLE_RATE * duration_ms * 2 / 1000;

    for (int i = 0; i < I2S_SAMPLE_RATE * duration_ms / 1000; i++) {
        // Generate a sine wave scaled to the full 32-bit signed integer range.
        float sample_f = volume * 0x7FFFFFFF * sinf(frequency * 2 * M_PI * i / I2S_SAMPLE_RATE);
        int32_t sample = (int32_t)sample_f;
        continuous_audio_file[2 * i] = sample;
        continuous_audio_file[2 * i + 1] = sample;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    speaker->play_oneshot(continuous_audio_file, continuous_buffer_size);

}