/*
 * Copyright (c) 2021 Valentin Milea <valentin.milea@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <fm_rda5807.h>
#include <rds_parser.h>
#include <hardware/i2c.h>
#include <pico/stdlib.h>
#include <stdio.h>

static const uint SDIO_PIN = PICO_DEFAULT_I2C_SDA_PIN;
static const uint SCLK_PIN = PICO_DEFAULT_I2C_SCL_PIN;

// change this to match your local stations
static const float STATION_PRESETS[] = {
    88.8f, // Radio Romania Actualitati
    90.4f, // EBS
    91.7f, // RFI
    95.6f, // Radio Cluj
    101.0f, // Radio Romania Cultural
    107.3f, // Itsy Bitsy
};
static_assert(count_of(STATION_PRESETS) <= 9, "");

#define DEFAULT_FREQUENCY STATION_PRESETS[0]

// change this to configure FM band, channel spacing, and de-emphasis
#define FM_CONFIG fm_config_europe()

static rda5807_t radio;
static rds_parser_t rds_parser;

static void print_help() {
    puts("RDA5807 - test program");
    puts("======================");
    puts("- =   Volume down / up");
    puts("1-9   Station presets");
    puts("{ }   Frequency down / up");
    puts("[ ]   Seek down / up");
    puts("<     Reduce seek threshold");
    puts(">     Increase seek threshold");
    puts("0     Toggle mute");
    puts("f     Toggle softmute");
    puts("m     Toggle mono");
    puts("b     Toggle bass boost");
    puts("i     Print station info");
    puts("r     Print RDS info");
    puts("x     Power down");
    puts("?     Print help");
    puts("");
}

static void print_station_info() {
    printf("%.2f MHz, RSSI: %u, stereo: %u\n",
        fm_get_frequency(&radio),
        fm_get_rssi(&radio),
        fm_get_stereo_indicator(&radio));
}

static void print_rds_info() {
    char program_id_str[5];
    rds_get_program_id_as_str(&rds_parser, program_id_str);
    printf("RDS - PI: %s, PTY: %u, DI_PTY: %u, DI_ST: %u, MS: %u, TP: %u, TA: %u\n",
        program_id_str,
        rds_get_program_type(&rds_parser),
        rds_has_dynamic_program_type(&rds_parser),
        rds_has_stereo(&rds_parser),
        rds_has_music(&rds_parser),
        rds_has_traffic_program(&rds_parser),
        rds_has_traffic_announcement(&rds_parser));
    printf("      PS: %s\n", rds_get_program_service_name_str(&rds_parser));

#if RDS_PARSER_RADIO_TEXT_ENABLE
    printf("      RT: %u-'%s'\n",
        rds_has_alternative_radio_text(&rds_parser),
        rds_get_radio_text_str(&rds_parser));
#endif

#if RDS_PARSER_ALTERNATIVE_FREQUENCIES_ENABLE
    size_t alt_freq_count = rds_get_alternative_frequency_count(&rds_parser);
    printf("      ALT: %zu", alt_freq_count);
    if (0 < alt_freq_count) {
        printf(" -- ");
        for (size_t i = 0; i < alt_freq_count; i++) {
            uint8_t freq = rds_get_alternative_frequency(&rds_parser, i);
            printf("%.1f", rds_decode_alternative_frequency(freq));
            if (i + 1 < alt_freq_count) {
                printf(", ");
            }
        }
        puts(" MHz");
    } else {
        puts("");
    }
#endif
}

static void update_rds() {
    union
    {
        uint16_t group_data[4];
        rds_group_t group;
    } rds;
    if (fm_read_rds_group(&radio, rds.group_data)) {
        rds_parser_update(&rds_parser, &rds.group);
    }
}

static void set_frequency(float frequency) {
    fm_set_frequency_blocking(&radio, frequency);
    printf("%.2f MHz\n", fm_get_frequency(&radio));
    rds_parser_reset(&rds_parser);
}

static void seek(fm_seek_direction_t direction) {
    // The easiest way to seek would be with fm_seek_blocking(). The async version
    // frees up the CPU for other work. Here we just print the current frequency
    // every 100ms until a new station has been found.

    fm_seek_async(&radio, direction);

    puts("Seeking...");
    fm_async_progress_t progress;
    do {
        sleep_ms(100);
        progress = fm_async_task_tick(&radio);
        printf("... %.2f MHz\n", fm_get_frequency(&radio));
    } while (!progress.done);

    if (progress.result == 0) {
        puts("... finished");
    } else {
        printf("... failed: %d\n", progress.result);
    }
    rds_parser_reset(&rds_parser);
}

static void loop() {
    int result = getchar_timeout_us(0);
    if (result != PICO_ERROR_TIMEOUT) {
        // handle command
        if (fm_is_powered_up(&radio)) {
            char ch = (char)result;
            if (ch == '-') {
                if (0 < fm_get_volume(&radio)) {
                    fm_set_volume(&radio, fm_get_volume(&radio) - 1);
                    printf("Set volume: %u\n", fm_get_volume(&radio));
                }
            } else if (ch == '=') {
                if (fm_get_volume(&radio) < 30) {
                    fm_set_volume(&radio, fm_get_volume(&radio) + 1);
                    printf("Set volume: %u\n", fm_get_volume(&radio));
                }
            } else if ('0' < ch && ch <= '0' + count_of(STATION_PRESETS)) {
                float frequency = STATION_PRESETS[ch - '1'];
                set_frequency(frequency);
            } else if (ch == '{') {
                fm_frequency_range_t range = fm_get_frequency_range(&radio);
                float frequency = fm_get_frequency(&radio) - range.spacing;
                if (frequency < range.bottom) {
                    frequency = range.top; // wrap to top
                }
                set_frequency(frequency);
            } else if (ch == '}') {
                fm_frequency_range_t range = fm_get_frequency_range(&radio);
                float frequency = fm_get_frequency(&radio) + range.spacing;
                if (range.top < frequency) {
                    frequency = range.bottom; // wrap to bottom
                }
                set_frequency(frequency);
            } else if (ch == '[') {
                seek(FM_SEEK_DOWN);
            } else if (ch == ']') {
                seek(FM_SEEK_UP);
            } else if (ch == '<') {
                if (0 < fm_get_seek_threshold(&radio)) {
                    fm_set_seek_threshold(&radio, fm_get_seek_threshold(&radio) - 1);
                    printf("Set seek threshold: %u\n", fm_get_seek_threshold(&radio));
                }
            } else if (ch == '>') {
                if (fm_get_seek_threshold(&radio) < FM_MAX_SEEK_THRESHOLD) {
                    fm_set_seek_threshold(&radio, fm_get_seek_threshold(&radio) + 1);
                    printf("Set seek threshold: %u\n", fm_get_seek_threshold(&radio));
                }
            } else if (ch == '0') {
                fm_set_mute(&radio, !fm_get_mute(&radio));
                printf("Set mute: %u\n", fm_get_mute(&radio));
            } else if (ch == 'f') {
                fm_set_softmute(&radio, !fm_get_softmute(&radio));
                printf("Set softmute: %u\n", fm_get_softmute(&radio));
            } else if (ch == 'm') {
                fm_set_mono(&radio, !fm_get_mono(&radio));
                printf("Set mono: %u\n", fm_get_mono(&radio));
            } else if (ch == 'b') {
                fm_set_bass_boost(&radio, !fm_get_bass_boost(&radio));
                printf("Set bass boost: %u\n", fm_get_bass_boost(&radio));
            } else if (ch == 'i') {
                print_station_info();
            } else if (ch == 'r') {
                print_rds_info();
            } else if (ch == 'x') {
                if (fm_is_powered_up(&radio)) {
                    puts("Power down");
                    fm_power_down(&radio);
                    rds_parser_reset(&rds_parser);
                }
            } else if (ch == '?') {
                print_help();
            }
        } else {
            puts("Power up");
            fm_power_up(&radio, FM_CONFIG);
        }
    }

    if (fm_is_powered_up(&radio)) {
        update_rds(&radio);
    }
    sleep_ms(40);
}

int main() {
    stdio_init_all();
    print_help();

    // RDA5807 supports up to 400kHz SCLK frequency
    i2c_init(i2c_default, 400 * 1000);

    fm_init(&radio, i2c_default, SDIO_PIN, SCLK_PIN, true /* enable_pull_ups */);
    sleep_ms(500); // wait for radio IC to initialize
    fm_power_up(&radio, FM_CONFIG);
    fm_set_frequency_blocking(&radio, DEFAULT_FREQUENCY);
    fm_set_volume(&radio, 1);
    fm_set_mute(&radio, false);

    rds_parser_reset(&rds_parser);
    do {
        loop();
    } while (true);
}
