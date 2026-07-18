/*
 * Analog microphone pitch to motor PWM controller for ESP32-C3.
 *
 * Microphone analog output: GPIO0 / ADC1_CH0
 * Motor-driver PWM input: GPIO6 / LEDC channel 0
 */
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>

#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MICROPHONE_ADC_UNIT          ADC_UNIT_1
#define MICROPHONE_ADC_CHANNEL       ADC_CHANNEL_0  /* GPIO0 on ESP32-C3 */
#define MOTOR_PWM_GPIO               6
#define MOTOR_PWM_MODE               LEDC_LOW_SPEED_MODE
#define MOTOR_PWM_TIMER              LEDC_TIMER_0
#define MOTOR_PWM_CHANNEL            LEDC_CHANNEL_0
#define MOTOR_PWM_FREQUENCY_HZ       20000
#define MOTOR_PWM_RESOLUTION          LEDC_TIMER_10_BIT
#define MOTOR_PWM_MAX_DUTY           ((1U << 10) - 1U)

#define SAMPLE_COUNT                 1024
#define MIN_SIGNAL_PEAK_TO_PEAK      80
#define MIN_AUTOCORRELATION_PERCENT  30
#define INVALID_FRAME_LIMIT          3
#define FREQUENCY_FILTER_DIVISOR     8
#define REPORT_INTERVAL_MS           150

#define MIN_FREQUENCY_HZ             CONFIG_AUDIO_MOTOR_MIN_FREQUENCY_HZ
#define MAX_FREQUENCY_HZ             CONFIG_AUDIO_MOTOR_MAX_FREQUENCY_HZ
#define MIN_DUTY_PERCENT             CONFIG_AUDIO_MOTOR_MIN_DUTY_PERCENT
#define MAX_DUTY_PERCENT             CONFIG_AUDIO_MOTOR_MAX_DUTY_PERCENT

static const char *TAG = "audio_motor";

typedef struct {
    uint32_t raw_frequency_hz;
    uint32_t filtered_frequency_hz;
    uint16_t peak_to_peak;
} audio_measurement_t;

static adc_oneshot_unit_handle_t adc_handle;
static uint32_t previous_frequency_hz;
static uint8_t consecutive_invalid_frames;

static void initialize_microphone_adc(void)
{
    const adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = MICROPHONE_ADC_UNIT,
    };
    const adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &adc_handle));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, MICROPHONE_ADC_CHANNEL,
                                               &channel_config));
}

static void initialize_motor_pwm(void)
{
    const ledc_timer_config_t timer_config = {
        .speed_mode = MOTOR_PWM_MODE,
        .duty_resolution = MOTOR_PWM_RESOLUTION,
        .timer_num = MOTOR_PWM_TIMER,
        .freq_hz = MOTOR_PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    const ledc_channel_config_t channel_config = {
        .gpio_num = MOTOR_PWM_GPIO,
        .speed_mode = MOTOR_PWM_MODE,
        .channel = MOTOR_PWM_CHANNEL,
        .timer_sel = MOTOR_PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
    };

    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

static uint32_t smooth_frequency(uint32_t raw_frequency_hz)
{
    if (raw_frequency_hz == 0) {
        if (consecutive_invalid_frames < INVALID_FRAME_LIMIT) {
            ++consecutive_invalid_frames;
            return previous_frequency_hz;
        }
        previous_frequency_hz = 0;
        return 0;
    }

    consecutive_invalid_frames = 0;
    if (previous_frequency_hz == 0) {
        previous_frequency_hz = MIN_FREQUENCY_HZ;
    }
    /* An 87.5/12.5 low-pass filter makes motor speed changes gradual. */
    previous_frequency_hz =
        (previous_frequency_hz * (FREQUENCY_FILTER_DIVISOR - 1U) + raw_frequency_hz) /
        FREQUENCY_FILTER_DIVISOR;
    return previous_frequency_hz;
}

static audio_measurement_t measure_audio(void)
{
    uint16_t samples[SAMPLE_COUNT];
    uint32_t total = 0;
    uint16_t minimum = UINT16_MAX;
    uint16_t maximum = 0;
    const int64_t start_us = esp_timer_get_time();

    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        int raw_value = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, MICROPHONE_ADC_CHANNEL, &raw_value));
        samples[i] = (uint16_t)raw_value;
        total += samples[i];
        if (samples[i] < minimum) {
            minimum = samples[i];
        }
        if (samples[i] > maximum) {
            maximum = samples[i];
        }
    }

    const int64_t elapsed_us = esp_timer_get_time() - start_us;
    const uint16_t peak_to_peak = maximum - minimum;
    audio_measurement_t result = {.peak_to_peak = peak_to_peak};
    if (peak_to_peak < MIN_SIGNAL_PEAK_TO_PEAK || elapsed_us <= 0) {
        result.filtered_frequency_hz = smooth_frequency(0);
        return result;
    }

    const uint16_t centre = total / SAMPLE_COUNT;
    const uint32_t sample_rate_hz = (SAMPLE_COUNT * 1000000ULL) / elapsed_us;
    uint32_t minimum_lag = sample_rate_hz / MAX_FREQUENCY_HZ;
    uint32_t maximum_lag = sample_rate_hz / MIN_FREQUENCY_HZ;
    if (minimum_lag < 2) {
        minimum_lag = 2;
    }
    if (maximum_lag > SAMPLE_COUNT / 2) {
        maximum_lag = SAMPLE_COUNT / 2;
    }
    if (minimum_lag >= maximum_lag) {
        result.filtered_frequency_hz = smooth_frequency(0);
        return result;
    }

    int64_t energy = 0;
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        const int32_t sample = (int32_t)samples[i] - centre;
        energy += (int64_t)sample * sample;
    }
    energy /= SAMPLE_COUNT;
    if (energy == 0) {
        result.filtered_frequency_hz = smooth_frequency(0);
        return result;
    }

    uint32_t detected_lag = 0;
    int64_t previous_correlation = INT64_MIN;
    int64_t current_correlation = INT64_MIN;
    for (uint32_t lag = minimum_lag; lag <= maximum_lag; ++lag) {
        int64_t correlation = 0;
        for (int i = 0; i < SAMPLE_COUNT - lag; ++i) {
            const int32_t first = (int32_t)samples[i] - centre;
            const int32_t second = (int32_t)samples[i + lag] - centre;
            correlation += (int64_t)first * second;
        }
        correlation /= SAMPLE_COUNT - lag;

        if (lag > minimum_lag && current_correlation > previous_correlation &&
            current_correlation >= correlation &&
            current_correlation * 100 >= energy * MIN_AUTOCORRELATION_PERCENT) {
            detected_lag = lag - 1;
            break;
        }
        previous_correlation = current_correlation;
        current_correlation = correlation;
    }

    if (detected_lag != 0) {
        result.raw_frequency_hz = sample_rate_hz / detected_lag;
    }
    result.filtered_frequency_hz = smooth_frequency(result.raw_frequency_hz);
    return result;
}

static uint32_t duty_for_frequency(uint32_t frequency_hz)
{
    if (frequency_hz == 0) {
        return 0;
    }

    const uint32_t clamped_frequency = frequency_hz > MAX_FREQUENCY_HZ
                                           ? MAX_FREQUENCY_HZ
                                           : frequency_hz;
    const uint32_t duty_percent = MIN_DUTY_PERCENT +
        ((clamped_frequency - MIN_FREQUENCY_HZ) *
         (MAX_DUTY_PERCENT - MIN_DUTY_PERCENT)) /
        (MAX_FREQUENCY_HZ - MIN_FREQUENCY_HZ);
    return (duty_percent * MOTOR_PWM_MAX_DUTY) / 100U;
}

void app_main(void)
{
    initialize_microphone_adc();
    initialize_motor_pwm();

    ESP_LOGI(TAG, "Microphone GPIO0 -> ADC1_CH0; motor PWM GPIO%d at %d Hz",
             MOTOR_PWM_GPIO, MOTOR_PWM_FREQUENCY_HZ);

    while (true) {
        const audio_measurement_t audio = measure_audio();
        const uint32_t duty = duty_for_frequency(audio.filtered_frequency_hz);
        const uint32_t duty_percent = (duty * 100U) / MOTOR_PWM_MAX_DUTY;

        ESP_ERROR_CHECK(ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CHANNEL, duty));
        ESP_ERROR_CHECK(ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CHANNEL));
        ESP_LOGI(TAG,
                 "raw_pitch=%" PRIu32 " Hz, pitch=%" PRIu32
                 " Hz, level=%u, duty=%" PRIu32 "/%u (%" PRIu32 "%%)",
                 audio.raw_frequency_hz, audio.filtered_frequency_hz,
                 audio.peak_to_peak, duty, MOTOR_PWM_MAX_DUTY, duty_percent);

        vTaskDelay(pdMS_TO_TICKS(REPORT_INTERVAL_MS));
    }
}
