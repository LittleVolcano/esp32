idf_component_register(SRCS "audio_motor_pwm.c"
                    PRIV_REQUIRES esp_adc esp_driver_ledc esp_timer
                    INCLUDE_DIRS ".")
