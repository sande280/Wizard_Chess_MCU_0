#pragma once

#include <stdio.h>
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <stdlib.h>
#include <cmath>
#include <cstdint>

#define I2S_PORT                I2S_NUM_0

//I2S Pinout
#define I2S_MCK_IO              (GPIO_NUM_NC)  // Master Clock
#define I2S_BCLK_IO             (GPIO_NUM_18)  // Bit Clock
#define I2S_WS_IO               (GPIO_NUM_17)  // Word Select (Left/Right Clock)
#define I2S_DOUT_IO             (GPIO_NUM_15)  // Data Out (to Codec/DAC DIN)
#define I2S_DIN_IO              (GPIO_NUM_16)  // Data In (from Codec/ADC DOUT)

// Audio Parameters
#define I2S_SAMPLE_RATE         (16000)   // Sample rate for voice
#define I2S_BITS_PER_SAMPLE     (I2S_DATA_BIT_WIDTH_32BIT)      // 32-bit audio
#define I2S_CHANNEL_FORMAT      (I2S_SLOT_MODE_STEREO) // Stereo

class audio
{
private:
    /**
     * Sets up GPIO for controlling the PAM8006 Audio amplifier
     * GPIO3=Mute
     * GPIO46=SD
     */
    void init_gpio();

    /**
     * Sets up the I2S line for sending audio data
     */
    void init_i2s();

    /**
     * @brief The entry point for the continuous playback task.
     * @param pvParameters A pointer to the audio class instance (`this`).
     */
    static void playback_task(void* pvParameters);

    i2s_chan_handle_t tx_handle;
    i2s_chan_handle_t rx_handle;

    // Members for continuous playback task
    TaskHandle_t m_playback_task_handle = NULL;
    int32_t* m_continuous_audio_buffer = NULL;
    uint32_t m_continuous_buffer_size = 0;

    // Members for one-shot playback
    volatile bool m_play_oneshot_flag = false;
    int32_t* m_oneshot_audio_buffer = NULL;
    uint32_t m_oneshot_buffer_size = 0;

public:
    /**
     * Initializes the audio output functionality
     */
    void init();

    /**
     * Writes a single buffer of audio data to the I2S driver.
     * This is a blocking call.
     */
    void play_sound(int32_t* audio_file, uint32_t audio_size);

    /**
     * Starts a background task to play an audio buffer continuously.
     * @param audio_buffer The buffer containing audio data to loop.
     * @param buffer_size The size of the buffer in 32-bit words.
     */
    void start_continuous_playback(int32_t* audio_buffer, uint32_t buffer_size);

    /**
     * Stops the continuous playback background task.
     */
    void stop_continuous_playback();

    /**
     * Plays a sound one time, interrupting the continuous playback.
     * @param audio_buffer The buffer containing the one-shot sound.
     * @param buffer_size The size of the buffer in 32-bit words.
     */
    void play_oneshot(int32_t* audio_buffer, uint32_t buffer_size);

    /**
     * Sets the mute pin on the audio amplifier for power savings
     * True = Outputs off
     */
    void set_mute(bool mute);

};

audio* speaker;