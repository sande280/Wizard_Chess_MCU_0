#include "audio.hpp"
#include <cstdint>

// Define TAG for logging
static const char* TAG = "AUDIO";

// Declare external audio data
// Make sure these match the definition in your data file
extern const int16_t captureAudio[];
extern const uint32_t captureAudioSize;

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
    // Increase DMA buffers for stability
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 240;
    
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle)); // Assign to global handles

    // Configure the I2S channel
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_CHANNEL_FORMAT),
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

void audio::play_sound(const int32_t* audio_file, uint32_t audio_size)
{
    size_t bytes_written = 0;
    // Using a large timeout helps prevent popping if the thread gets preempted briefly
    esp_err_t ret = i2s_channel_write(tx_handle, audio_file, audio_size * sizeof(int32_t), &bytes_written, portMAX_DELAY);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S Write Error: %s", esp_err_to_name(ret));
    }
}

void audio::set_mute(bool mute)
{
    gpio_set_level(GPIO_NUM_3, mute ? 1 : 0);
}

void audio::playback_task(void* pvParameters)
{
    audio* instance = (audio*)pvParameters;
    ESP_LOGI(TAG, "Starting playback task.");
    
    // Silence buffer for when idle
    const uint32_t silence_len = 100;
    int32_t silence[silence_len * 2] = {0};

    // Conversion buffer for 16-bit to 32-bit expansion
    const size_t CHUNK_SIZE = 2048; 
    int32_t convert_buffer[CHUNK_SIZE * 2]; 

    while(1)
    {
        if (instance->m_play_oneshot_flag)
        {
            if (instance->m_oneshot_is_16bit) {
                // Handle 16-bit conversion on the fly
                // This prevents large memory allocations
                const int16_t* src = instance->m_oneshot_audio_buffer.buffer16;
                uint32_t total_samples = instance->m_oneshot_buffer_size;
                
                size_t processed = 0;
                while (processed < total_samples) {
                    size_t chunk_len = 0;
                    // Process a small chunk
                    for (size_t i = 0; i < CHUNK_SIZE && processed < total_samples; i++, processed++) {
                        int16_t sample16 = src[processed];
                        // Convert to 32-bit (shift left 16)
                        int32_t sample32 = (int32_t)sample16 << 16;
                        
                        // Stereo duplication
                        convert_buffer[i * 2] = sample32;
                        convert_buffer[i * 2 + 1] = sample32;
                        
                        chunk_len++;
                    }
                    
                    // Write chunk to I2S
                    size_t bytes_written;
                    i2s_channel_write(instance->tx_handle, convert_buffer, chunk_len * 2 * sizeof(int32_t), &bytes_written, portMAX_DELAY);
                }
            } else {
                // Direct 32-bit play
                instance->play_sound(instance->m_oneshot_audio_buffer.buffer32, instance->m_oneshot_buffer_size);
            }

            instance->m_play_oneshot_flag = false;
        }
        else if (instance->m_continuous_audio_buffer != NULL && instance->m_continuous_buffer_size > 0)
        {
            instance->play_sound(instance->m_continuous_audio_buffer, instance->m_continuous_buffer_size);
        }
        else
        {
            // Keep pipeline primed
            size_t bytes_written = 0;
            i2s_channel_write(instance->tx_handle, silence, sizeof(silence), &bytes_written, 10);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void audio::start_continuous_playback(const int32_t* audio_buffer, uint32_t buffer_size)
{
    if (m_playback_task_handle != NULL) {
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
        this,
        5,
        &m_playback_task_handle
    );
}

void audio::stop_continuous_playback()
{
    m_continuous_audio_buffer = NULL;
    m_continuous_buffer_size = 0;
}

void audio::play_oneshot(const int32_t* audio_buffer, uint32_t buffer_size)
{
    if (m_play_oneshot_flag) return;
    
    m_oneshot_audio_buffer.buffer32 = audio_buffer;
    m_oneshot_buffer_size = buffer_size;
    m_oneshot_is_16bit = false;
    m_play_oneshot_flag = true;
}

void audio::play_oneshot(const int16_t* audio_buffer, uint32_t buffer_size)
{
    if (m_play_oneshot_flag) return;

    m_oneshot_audio_buffer.buffer16 = audio_buffer;
    m_oneshot_buffer_size = buffer_size;
    m_oneshot_is_16bit = true;
    m_play_oneshot_flag = true;
}

void audio::playCaptureSound()
{
    // Use the 16-bit overload to avoid large allocations
    // This calls play_oneshot(const int16_t*, ...) which uses the on-the-fly conversion logic
    play_oneshot(captureAudio, captureAudioSize);
}