#include "audio.hpp"
#include <vector>
#include <cmath>

// Define TAG for logging
static const char* TAG_TONE = "AUDIO_TONE";

// Pre-defined one-shot audio buffer
int32_t* tone_buffer = NULL;
uint32_t tone_buffer_size = 0;

/**
 * Generates a sine wave tone and stores it in a dynamically allocated buffer.
 * The buffer is intended to be used with audio::play_oneshot.
 *
 * @param frequency The frequency of the tone in Hz.
 * @param duration_ms The duration of the tone in milliseconds.
 * @param amplitude The amplitude of the tone (0.0 to 1.0).
 */
void generate_tone(float frequency, uint32_t duration_ms, float amplitude) {
    if (tone_buffer != NULL) {
        free(tone_buffer);
        tone_buffer = NULL;
        tone_buffer_size = 0;
    }

    uint32_t num_samples = (I2S_SAMPLE_RATE * duration_ms) / 1000;
    // Align to stereo samples (2 channels)
    if (num_samples % 2 != 0) num_samples++; 
    
    // Allocate buffer for 32-bit stereo samples
    // Each frame consists of 2 channels (Left + Right)
    // However, I2S_CHANNEL_FMT_RIGHT_LEFT implies interleaved L/R
    // Total 32-bit words = num_samples * 2 (stereo)
    // Wait, the configured slot mask and bit width matters.
    // Assuming standard I2S Philips or MSB, 2 slots.
    // Let's assume standard interleaved stereo 32-bit samples.
    
    tone_buffer_size = num_samples * 2; 
    tone_buffer = (int32_t*)malloc(tone_buffer_size * sizeof(int32_t));

    if (tone_buffer == NULL) {
        ESP_LOGE(TAG_TONE, "Failed to allocate memory for tone buffer");
        return;
    }

    float max_val = 2147483647.0f * amplitude; // Max 32-bit signed int
    float phase_increment = (2.0f * M_PI * frequency) / I2S_SAMPLE_RATE;
    float phase = 0.0f;

    for (uint32_t i = 0; i < num_samples; i++) {
        int32_t sample_val = (int32_t)(sin(phase) * max_val);
        
        // Interleaved stereo: Left channel
        tone_buffer[i * 2] = sample_val;
        // Right channel (duplicate for mono sound on both speakers)
        tone_buffer[i * 2 + 1] = sample_val;

        phase += phase_increment;
        if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
    }

    ESP_LOGI(TAG_TONE, "Generated tone: %d Hz, %d ms, buffer size: %lu bytes", (int)frequency, (int)duration_ms, tone_buffer_size * sizeof(int32_t));
}


// Example usage function to be called from main or elsewhere
void play_error_tone() {
    // Generate a short beep: 440Hz for 200ms
    generate_tone(440.0f, 2000, 0.0625f);
    
    if (speaker != nullptr && tone_buffer != nullptr) {
        speaker->play_oneshot(tone_buffer, tone_buffer_size);
    }
}