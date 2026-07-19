#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_PORT I2C_NUM_0
#define I2C_SDA_GPIO 4
#define I2C_SCL_GPIO 3
#define I2C_TIMEOUT_MS 100

#define MPU6050_ADDRESS 0x68
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_WHO_AM_I 0x75
#define MPU6050_WHO_AM_I_VALUE 0x68

/* Common settings for a 0.91 inch SSD1306 128x32 I2C OLED module. */
#define OLED_ADDRESS_PRIMARY 0x3C
#define OLED_ADDRESS_SECONDARY 0x3D
#define OLED_WIDTH 128
#define OLED_HEIGHT 32
#define OLED_PAGE_COUNT (OLED_HEIGHT / 8)
#define OLED_BUFFER_SIZE (OLED_WIDTH * OLED_PAGE_COUNT)
#define ACCEL_BAR_X 4
#define ACCEL_BAR_WIDTH (OLED_WIDTH - 2 * ACCEL_BAR_X)
#define ACCEL_BAR_TOP 8
#define ACCEL_BAR_BOTTOM 11
#define ACCEL_BAR_RANGE_G 1.5f
#define CAT_TENSE_THRESHOLD_G 0.50f
#define CAT_X 48
#define CAT_Y 14
#define CAT_WIDTH 32
#define CAT_HEIGHT 18
#define CAT_ANIMATION_FRAME_MS 200
#define ACCEL_DEADBAND_G 0.02f
#define DYNAMIC_FILTER_ALPHA 0.75f
#define MOTION_CONFIRM_DURATION_MS 200
#define DISPLAY_REFRESH_MS 100
#define INITIAL_BASELINE_SAMPLES 20
#define CONSOLE_REPORT_INTERVAL_MS 500
#define PEAK_HISTORY_DURATION_MS 5000
#define PEAK_HISTORY_SAMPLES (PEAK_HISTORY_DURATION_MS / DISPLAY_REFRESH_MS)
#define PEAK_FLASH_THRESHOLD_G 1.0f
#define PEAK_FLASH_DURATION_MS 500

static const char *TAG = "mpu6050";
static uint8_t oled_buffer[OLED_BUFFER_SIZE];
static uint8_t oled_address;
static bool oled_available;
static bool oled_inverted;
static bool baseline_ready;
static bool dynamic_filter_ready;
static bool dynamic_motion_confirmed;
static float initial_magnitude_sum_g;
static float initial_magnitude_g;
static float filtered_acceleration_change_g;

static float acceleration_history[PEAK_HISTORY_SAMPLES];
static float recent_peak_acceleration;
static uint8_t acceleration_history_index;
static uint8_t acceleration_history_count;
static uint16_t peak_flash_remaining_ms;
static bool peak_flash_phase;
static uint16_t motion_duration_ms;
static uint8_t initial_baseline_sample_count;
static uint8_t cat_animation_ticks;
static bool cat_animation_frame;

typedef struct {
    float ax;
    float ay;
    float az;
    float acceleration_change_g;
    float temperature;
    float gx;
    float gy;
    float gz;
} mpu6050_measurements_t;

static int16_t read_be16(const uint8_t *data)
{
    return (int16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static void i2c_write(uint8_t address, const uint8_t *data, size_t length)
{
    ESP_ERROR_CHECK(i2c_master_write_to_device(I2C_PORT, address, data, length,
                                               pdMS_TO_TICKS(I2C_TIMEOUT_MS)));
}

static void write_mpu6050_register(uint8_t reg, uint8_t value)
{
    const uint8_t message[] = {reg, value};
    i2c_write(MPU6050_ADDRESS, message, sizeof(message));
}

static void read_mpu6050_registers(uint8_t reg, uint8_t *data, size_t length)
{
    ESP_ERROR_CHECK(i2c_master_write_read_device(I2C_PORT, MPU6050_ADDRESS, &reg, 1,
                                                  data, length,
                                                  pdMS_TO_TICKS(I2C_TIMEOUT_MS)));
}

static void initialize_i2c(void)
{
    const i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &config));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, config.mode, 0, 0, 0));
}

static void initialize_mpu6050(void)
{
    uint8_t device_id = 0;
    read_mpu6050_registers(MPU6050_REG_WHO_AM_I, &device_id, 1);
    ESP_ERROR_CHECK(device_id == MPU6050_WHO_AM_I_VALUE ? ESP_OK : ESP_ERR_NOT_FOUND);
    write_mpu6050_register(MPU6050_REG_PWR_MGMT_1, 0x00);
    ESP_LOGI(TAG, "MPU6050 detected at 0x%02X", MPU6050_ADDRESS);
}

static esp_err_t oled_write_command(uint8_t command)
{
    const uint8_t message[] = {0x00, command};
    return i2c_master_write_to_device(I2C_PORT, oled_address, message, sizeof(message),
                                      pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

static esp_err_t oled_write_data(const uint8_t *data, size_t length)
{
    uint8_t message[17] = {0x40};
    while (length > 0) {
        const size_t chunk_length = length > sizeof(message) - 1 ? sizeof(message) - 1 : length;
        memcpy(&message[1], data, chunk_length);
        const esp_err_t result = i2c_master_write_to_device(I2C_PORT, oled_address, message,
                                                             chunk_length + 1,
                                                             pdMS_TO_TICKS(I2C_TIMEOUT_MS));
        if (result != ESP_OK) {
            return result;
        }
        data += chunk_length;
        length -= chunk_length;
    }
    return ESP_OK;
}

static esp_err_t oled_set_inverted(bool inverted)
{
    const esp_err_t result = oled_write_command(inverted ? 0xA7 : 0xA6);
    if (result == ESP_OK) {
        oled_inverted = inverted;
    }
    return result;
}

static void initialize_oled(void)
{
    static const uint8_t commands[] = {
        0xAE, 0xD5, 0x80, 0xA8, OLED_HEIGHT - 1, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x02,
        0x81, 0x8F, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF,
    };
    static const uint8_t addresses[] = {OLED_ADDRESS_PRIMARY, OLED_ADDRESS_SECONDARY};

    for (size_t candidate = 0; candidate < sizeof(addresses); ++candidate) {
        oled_address = addresses[candidate];
        bool initialized = true;
        for (size_t i = 0; i < sizeof(commands); ++i) {
            if (oled_write_command(commands[i]) != ESP_OK) {
                initialized = false;
                break;
            }
        }
        if (initialized) {
            oled_available = true;
            ESP_LOGI(TAG, "SSD1306 OLED detected at 0x%02X", oled_address);
            return;
        }
    }

    ESP_LOGW(TAG, "No OLED response at 0x%02X or 0x%02X; console output remains active",
             OLED_ADDRESS_PRIMARY, OLED_ADDRESS_SECONDARY);
}

static void oled_set_pixel(uint8_t x, uint8_t y)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }
    oled_buffer[(y / 8) * OLED_WIDTH + x] |= 1U << (y % 8);
}

static const uint8_t *glyph_for_character(char character)
{
    static const uint8_t blank[] = {0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t digits[][5] = {
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
        {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
        {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
        {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
    };
    static const uint8_t plus[] = {0x08, 0x08, 0x3E, 0x08, 0x08};
    static const uint8_t minus[] = {0x08, 0x08, 0x08, 0x08, 0x08};
    static const uint8_t dot[] = {0x00, 0x60, 0x60, 0x00, 0x00};
    static const uint8_t colon[] = {0x00, 0x36, 0x36, 0x00, 0x00};
    static const uint8_t letter_a[] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
    static const uint8_t letter_c[] = {0x3E, 0x41, 0x41, 0x41, 0x22};
    static const uint8_t letter_d[] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
    static const uint8_t letter_g[] = {0x3E, 0x41, 0x49, 0x49, 0x7A};
    static const uint8_t letter_p[] = {0x7F, 0x09, 0x09, 0x09, 0x06};
    static const uint8_t letter_s[] = {0x46, 0x49, 0x49, 0x49, 0x31};
    static const uint8_t letter_t[] = {0x01, 0x01, 0x7F, 0x01, 0x01};
    static const uint8_t letter_x[] = {0x63, 0x14, 0x08, 0x14, 0x63};
    static const uint8_t letter_y[] = {0x07, 0x08, 0x70, 0x08, 0x07};
    static const uint8_t letter_z[] = {0x61, 0x51, 0x49, 0x45, 0x43};

    if (character >= '0' && character <= '9') {
        return digits[character - '0'];
    }
    switch (character) {
    case '+': return plus;
    case '-': return minus;
    case '.': return dot;
    case ':': return colon;
    case 'A': return letter_a;
    case 'C': return letter_c;
    case 'D': return letter_d;
    case 'G': return letter_g;
    case 'P': return letter_p;
    case 'S': return letter_s;
    case 'T': return letter_t;
    case 'X': return letter_x;
    case 'Y': return letter_y;
    case 'Z': return letter_z;
    default: return blank;
    }
}

static void oled_draw_text(uint8_t x, uint8_t y, const char *text)
{
    while (*text != '\0' && x + 5 < OLED_WIDTH) {
        const uint8_t *glyph = glyph_for_character(*text++);
        for (uint8_t column = 0; column < 5; ++column) {
            for (uint8_t row = 0; row < 7; ++row) {
                if (glyph[column] & (1U << row)) {
                    oled_set_pixel(x + column, y + row);
                }
            }
        }
        x += 6;
    }
}

static void oled_draw_sprite(uint8_t x, uint8_t y, const char *const *rows, uint8_t height)
{
    for (uint8_t row = 0; row < height; ++row) {
        for (uint8_t column = 0; column < CAT_WIDTH; ++column) {
            if (rows[row][column] == '#') {
                oled_set_pixel(x + column, y + row);
            }
        }
    }
}

static void oled_draw_cat(float peak_acceleration_g)
{
    /* Dark outline layer rasterized from the user-provided reference image at 32x18. */
    static const char *const reference_lying_cat[] = {
        ".......................#........", "......#...............###.......",
        ".....###.............##.#.......", ".....#.##....#########..#.......",
        "....#...######..........#.......", "....#...................#.......",
        "....#....................#......", "....#....................#......",
        "....#.....................#.....", "...#......................#.....",
        "..#..................##....####.", ".#...#####.###.......##...#....#",
        ".#..##...##.##................##", "#..##....##.....######.........#",
        "#..#.....##.....#####.........##", "#........##..................##.",
        "##......##..................##..", "#.......#...................#...",
    };
    const bool tense = peak_acceleration_g >= CAT_TENSE_THRESHOLD_G;
    const uint8_t frame_count = CAT_ANIMATION_FRAME_MS / DISPLAY_REFRESH_MS;
    uint8_t x = CAT_X;

    if (tense) {
        ++cat_animation_ticks;
        if (cat_animation_ticks >= frame_count) {
            cat_animation_ticks = 0;
            cat_animation_frame = !cat_animation_frame;
        }
        x += cat_animation_frame ? 2 : 0;
    } else {
        cat_animation_ticks = 0;
        cat_animation_frame = false;
    }
    oled_draw_sprite(x, CAT_Y, reference_lying_cat, CAT_HEIGHT);
}

static int16_t accel_to_bar_x(float acceleration_g)
{
    const uint8_t bar_left = ACCEL_BAR_X;
    const float clamped_g = acceleration_g > ACCEL_BAR_RANGE_G ? ACCEL_BAR_RANGE_G :
                            acceleration_g;
    return bar_left + (int16_t)((clamped_g / ACCEL_BAR_RANGE_G) * (ACCEL_BAR_WIDTH - 3));
}

static void oled_draw_accel_bar(float acceleration_g, float peak_acceleration_g)
{
    char value_text[8] = {0};
    char peak_text[8] = {0};
    const uint8_t bar_left = ACCEL_BAR_X;
    const uint8_t bar_right = ACCEL_BAR_X + ACCEL_BAR_WIDTH - 1;
    const uint8_t bar_top = ACCEL_BAR_TOP;
    const uint8_t bar_bottom = ACCEL_BAR_BOTTOM;
    const int16_t marker = accel_to_bar_x(acceleration_g);
    const int16_t peak_marker = accel_to_bar_x(peak_acceleration_g);

    snprintf(value_text, sizeof(value_text), "%.1fG", acceleration_g);
    snprintf(peak_text, sizeof(peak_text), "%.1fG", peak_acceleration_g);
    oled_draw_text(0, 0, "ACC");
    oled_draw_text(22, 0, value_text);
    oled_draw_text(64, 0, "P");
    oled_draw_text(72, 0, peak_text);
    for (uint8_t x = bar_left; x <= bar_right; ++x) {
        oled_set_pixel(x, bar_top);
        oled_set_pixel(x, bar_bottom);
    }
    for (uint8_t row = bar_top; row <= bar_bottom; ++row) {
        oled_set_pixel(bar_left, row);
        oled_set_pixel(bar_right, row);
    }
    for (int16_t x = bar_left + 1; x <= marker; ++x) {
        for (uint8_t row = bar_top + 1; row < bar_bottom; ++row) {
            oled_set_pixel(x, row);
        }
    }
    /* The taller line is the largest absolute acceleration during the last 5 seconds. */
    for (uint8_t row = bar_top - 1; row <= bar_bottom; ++row) {
        oled_set_pixel(peak_marker, row);
    }
    oled_draw_cat(peak_acceleration_g);
}

static float vector_magnitude(float x, float y, float z)
{
    return sqrtf(x * x + y * y + z * z);
}

static float acceleration_change_magnitude(const mpu6050_measurements_t *measurements)
{
    return measurements->acceleration_change_g;
}

static void update_recent_acceleration_peak(float acceleration_g)
{
    acceleration_history[acceleration_history_index] = acceleration_g;
    acceleration_history_index = (acceleration_history_index + 1) % PEAK_HISTORY_SAMPLES;
    if (acceleration_history_count < PEAK_HISTORY_SAMPLES) {
        ++acceleration_history_count;
    }

    recent_peak_acceleration = acceleration_history[0];
    for (uint8_t i = 1; i < acceleration_history_count; ++i) {
        if (acceleration_history[i] > recent_peak_acceleration) {
            recent_peak_acceleration = acceleration_history[i];
        }
    }
}

static void filter_acceleration_change(mpu6050_measurements_t *measurements)
{
    const float raw_magnitude_g = measurements->acceleration_change_g;
    if (raw_magnitude_g >= ACCEL_DEADBAND_G) {
        motion_duration_ms = motion_duration_ms + DISPLAY_REFRESH_MS < MOTION_CONFIRM_DURATION_MS ?
                             motion_duration_ms + DISPLAY_REFRESH_MS : MOTION_CONFIRM_DURATION_MS;
        if (motion_duration_ms >= MOTION_CONFIRM_DURATION_MS) {
            dynamic_motion_confirmed = true;
        }
    } else {
        motion_duration_ms = 0;
    }

    if (!dynamic_filter_ready) {
        filtered_acceleration_change_g = raw_magnitude_g;
        dynamic_filter_ready = true;
    } else {
        filtered_acceleration_change_g = DYNAMIC_FILTER_ALPHA * filtered_acceleration_change_g +
                                         (1.0f - DYNAMIC_FILTER_ALPHA) * raw_magnitude_g;
    }

    if (dynamic_motion_confirmed && raw_magnitude_g < ACCEL_DEADBAND_G &&
        filtered_acceleration_change_g < ACCEL_DEADBAND_G) {
        dynamic_motion_confirmed = false;
    }
    if (!dynamic_motion_confirmed) {
        measurements->acceleration_change_g = 0.0f;
        return;
    }

    measurements->acceleration_change_g = filtered_acceleration_change_g;
}

static void calculate_acceleration_change(mpu6050_measurements_t *measurements)
{
    const float total_magnitude_g = vector_magnitude(measurements->ax, measurements->ay,
                                                      measurements->az);
    if (!baseline_ready) {
        initial_magnitude_sum_g += total_magnitude_g;
        ++initial_baseline_sample_count;
        measurements->acceleration_change_g = 0.0f;
        if (initial_baseline_sample_count == INITIAL_BASELINE_SAMPLES) {
            initial_magnitude_g = initial_magnitude_sum_g / INITIAL_BASELINE_SAMPLES;
            baseline_ready = true;
            ESP_LOGI(TAG, "Initial acceleration baseline: %.3fG", initial_magnitude_g);
        }
        return;
    }

    measurements->acceleration_change_g = fabsf(total_magnitude_g - initial_magnitude_g);
    filter_acceleration_change(measurements);
}

static esp_err_t oled_flush(void)
{
    for (uint8_t page = 0; page < OLED_PAGE_COUNT; ++page) {
        esp_err_t result = oled_write_command(0xB0 | page);
        if (result == ESP_OK) {
            result = oled_write_command(0x00);
        }
        if (result == ESP_OK) {
            result = oled_write_command(0x10);
        }
        if (result == ESP_OK) {
            result = oled_write_data(&oled_buffer[page * OLED_WIDTH], OLED_WIDTH);
        }
        if (result != ESP_OK) {
            return result;
        }
    }
    return ESP_OK;
}

static void display_measurements(const mpu6050_measurements_t *measurements)
{
    const float acceleration_g = acceleration_change_magnitude(measurements);
    const bool new_peak = acceleration_g > recent_peak_acceleration &&
                          acceleration_g > PEAK_FLASH_THRESHOLD_G;
    if (new_peak) {
        peak_flash_remaining_ms = PEAK_FLASH_DURATION_MS;
    }
    update_recent_acceleration_peak(acceleration_g);
    if (!oled_available) {
        return;
    }

    memset(oled_buffer, 0, sizeof(oled_buffer));

    oled_draw_accel_bar(acceleration_g, recent_peak_acceleration);
    if (oled_flush() != ESP_OK) {
        oled_available = false;
        ESP_LOGW(TAG, "OLED communication lost; console output remains active");
        return;
    }

    bool should_invert = false;
    if (peak_flash_remaining_ms > 0) {
        peak_flash_phase = !peak_flash_phase;
        should_invert = peak_flash_phase;
        peak_flash_remaining_ms = peak_flash_remaining_ms > DISPLAY_REFRESH_MS ?
                                  peak_flash_remaining_ms - DISPLAY_REFRESH_MS : 0;
    } else {
        peak_flash_phase = false;
    }
    if (should_invert != oled_inverted && oled_set_inverted(should_invert) != ESP_OK) {
        oled_available = false;
        ESP_LOGW(TAG, "OLED communication lost; console output remains active");
    }
}

static mpu6050_measurements_t read_measurements(void)
{
    uint8_t data[14] = {0};
    read_mpu6050_registers(MPU6050_REG_ACCEL_XOUT_H, data, sizeof(data));
    mpu6050_measurements_t measurements = {
        .ax = read_be16(&data[0]) / 16384.0f,
        .ay = read_be16(&data[2]) / 16384.0f,
        .az = read_be16(&data[4]) / 16384.0f,
        .temperature = read_be16(&data[6]) / 340.0f + 36.53f,
        .gx = read_be16(&data[8]) / 131.0f,
        .gy = read_be16(&data[10]) / 131.0f,
        .gz = read_be16(&data[12]) / 131.0f,
    };
    calculate_acceleration_change(&measurements);
    return measurements;
}

static void report_measurements(const mpu6050_measurements_t *measurements)
{
    ESP_LOGI(TAG,
             "accel_g=(%.2f, %.2f, %.2f) gyro_dps=(%.2f, %.2f, %.2f) temperature_c=%.2f",
             measurements->ax, measurements->ay, measurements->az,
             measurements->gx, measurements->gy, measurements->gz, measurements->temperature);
}

void app_main(void)
{
    uint32_t elapsed_since_console_report_ms = CONSOLE_REPORT_INTERVAL_MS;

    initialize_i2c();
    initialize_mpu6050();
    initialize_oled();

    while (true) {
        const mpu6050_measurements_t measurements = read_measurements();
        if (elapsed_since_console_report_ms >= CONSOLE_REPORT_INTERVAL_MS) {
            report_measurements(&measurements);
            elapsed_since_console_report_ms = 0;
        }
        display_measurements(&measurements);
        vTaskDelay(pdMS_TO_TICKS(DISPLAY_REFRESH_MS));
        elapsed_since_console_report_ms += DISPLAY_REFRESH_MS;
    }
}
