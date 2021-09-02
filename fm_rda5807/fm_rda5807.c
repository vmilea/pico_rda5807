/*
 * Copyright (c) 2021 Valentin Milea <valentin.milea@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "fm_rda5807_regs.h"
#include <fm_rda5807.h>
#include <hardware/i2c.h>
#include <pico/stdlib.h>
#include <math.h>
#include <string.h>

static const uint TUNE_POLL_INTERVAL_MS = 5;
static const uint SEEK_POLL_INTERVAL_MS = 200; // relatively large, to reduce electrical interference from I2C

//
// misc
//

static fm_frequency_range_t fm_frequency_range(fm_band_t band, fm_channel_spacing_t channel_spacing) {
    fm_frequency_range_t range;
    switch (band) {
    case FM_BAND_COMMON:
        range.bottom = 87.0f;
        range.top = 108.0f;
        break;
    case FM_BAND_JAPAN:
        range.bottom = 76.0f;
        range.top = 91.0f;
        break;
    case FM_BAND_JAPAN_WIDE:
        range.bottom = 76.0f;
        range.top = 108.0f;
        break;
    case FM_BAND_EAST_EUROPE:
        range.bottom = 50.0f;
        range.top = 76.0f;
        break;
    default: // FM_BAND_EAST_EUROPE_UPPER
        range.bottom = 65.0f;
        range.top = 76.0f;
        break;
    }
    switch (channel_spacing) {
    case FM_CHANNEL_SPACING_200:
        range.spacing = 0.2f;
        break;
    case FM_CHANNEL_SPACING_100:
        range.spacing = 0.1f;
        break;
    default: // FM_CHANNEL_SPACING_50
        range.spacing = 0.05f;
        break;
    }
    return range;
}

static float fm_channel_to_frequency(uint16_t channel, fm_frequency_range_t range) {
    return channel * range.spacing + range.bottom;
}

static uint16_t fm_frequency_to_channel(float frequency, fm_frequency_range_t range) {
    return (uint16_t)roundf((frequency - range.bottom) / range.spacing);
}

//
// register access
//

static bool fm_read_registers(i2c_inst_t *i2c_inst, uint16_t *regs, size_t n) {
    assert(n <= 6); // registers 0xA..0xF

    uint8_t buf[12];
    size_t data_size = n * sizeof(uint16_t);
    int result = i2c_read_blocking(i2c_inst, RDA5807_ADDR_SEQUENTIAL, buf, data_size, false);
    if (result != (int)data_size) {
        return false; // failed
    }
    uint16_t *p = regs + 0xA;
    for (size_t i = 0; i < data_size;) {
        uint16_t reg = buf[i++] << 8; // hi
        reg |= buf[i++]; // lo
        *p++ = reg;
    }
    return true;
}

static bool fm_read_registers_up_to(i2c_inst_t *i2c_inst, uint16_t *regs, uint8_t reg_index) {
    // read order: 0xA, 0xB, 0xC, 0xD, 0xE, 0xF
    assert(0xA <= reg_index && reg_index <= 0xF);

    size_t n = reg_index - 9;
    return fm_read_registers(i2c_inst, regs, n);
}

static bool fm_write_registers(i2c_inst_t *i2c_inst, uint16_t *regs, size_t n) {
    assert(n <= 7); // registers 0x2..0x8

    uint8_t buf[14];
    size_t data_size = n * sizeof(uint16_t);
    uint16_t *p = regs + 0x2;
    for (size_t i = 0; i < data_size;) {
        uint16_t reg = *p++;
        buf[i++] = reg >> 8; // hi
        buf[i++] = reg & 0xFF; // lo
    }
    int result = i2c_write_blocking(i2c_inst, RDA5807_ADDR_SEQUENTIAL, buf, data_size, false);
    return result == (int)data_size;
}

static bool fm_write_registers_up_to(i2c_inst_t *i2c_inst, uint16_t *regs, uint8_t reg_index) {
    // write order: 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8
    assert(0x2 <= reg_index && reg_index <= 0x8);

    size_t n = reg_index - 1;
    return fm_write_registers(i2c_inst, regs, n);
}

static bool fm_write_single_register(i2c_inst_t *i2c_inst, uint16_t *regs, size_t reg_index) {
    assert(0x2 <= reg_index && reg_index <= 0x8);

    if (reg_index == 0x2) {
        // no need for reg_index
        return fm_write_registers(i2c_inst, regs, 1);
    }
    uint16_t reg = regs[reg_index];
    uint8_t buf[3] = {reg_index, reg >> 8, reg & 0xFF};
    int result = i2c_write_blocking(i2c_inst, RDA5807_ADDR_RANDOM_ACCESS, buf, sizeof(buf), false);
    return result == sizeof(buf);
}

static bool fm_read_single_register(i2c_inst_t *i2c_inst, uint16_t *regs, size_t reg_index) {
    assert(reg_index <= 0xF);

    if (reg_index == 0xA) {
        // no need for reg_index
        return fm_read_registers(i2c_inst, regs, 1);
    }
    uint8_t buf[2] = {reg_index};
    int result = i2c_write_blocking(i2c_inst, RDA5807_ADDR_RANDOM_ACCESS, buf, 1, true /* nostop */);
    if (result != 1) {
        return false;
    }
    buf[0] = 0;
    result = i2c_read_blocking(i2c_inst, RDA5807_ADDR_RANDOM_ACCESS, buf, 2, false);
    if (result != 2) {
        return false;
    }
    uint16_t reg = (buf[0] << 8) | buf[1];
    regs[reg_index] = reg;
    return true;
}

#define fm_set_bit(reg, bit, value) \
    if (value) {                    \
        reg |= bit##_BIT;           \
    } else {                        \
        reg &= ~bit##_BIT;          \
    }

#define fm_get_bit(reg, bit) \
    (((reg)&bit##_BIT) != 0)

#define fm_set_bits(reg, bits, value) \
    reg &= ~bits##_BITS;              \
    reg |= (value) << bits##_LSB;

#define fm_get_bits(reg, bits) \
    (((reg)&bits##_BITS) >> bits##_LSB)

static void fm_set_channel_spacing_bits(uint16_t *regs, fm_channel_spacing_t channel_spacing) {
    switch (channel_spacing) {
    case FM_CHANNEL_SPACING_200:
        fm_set_bits(regs[0x3], SPACE, 0b01);
        break;
    case FM_CHANNEL_SPACING_100:
        fm_set_bits(regs[0x3], SPACE, 0b00);
        break;
    case FM_CHANNEL_SPACING_50:
        fm_set_bits(regs[0x3], SPACE, 0b10);
        break;
    default: // FM_CHANNEL_SPACING_25
        fm_set_bits(regs[0x3], SPACE, 0b11);
        break;
    }
}

static void fm_set_band_bits(uint16_t *regs, fm_band_t band) {
    switch (band) {
    case FM_BAND_COMMON:
        fm_set_bits(regs[0x3], BAND, 0b00);
        break;
    case FM_BAND_JAPAN:
        fm_set_bits(regs[0x3], BAND, 0b01);
        break;
    case FM_BAND_JAPAN_WIDE:
        fm_set_bits(regs[0x3], BAND, 0b10);
        break;
    case FM_BAND_EAST_EUROPE:
        fm_set_bits(regs[0x3], BAND, 0b11);
        fm_set_bit(regs[0x7], BAND_65M_50M_MODE, false);
        break;
    default: // FM_BAND_EAST_EUROPE_UPPER
        fm_set_bits(regs[0x3], BAND, 0b11);
        fm_set_bit(regs[0x7], BAND_65M_50M_MODE, true);
        break;
    }
}

//
// public interface
//

void fm_init(rda5807_t *radio, i2c_inst_t *i2c_inst, uint8_t sdio_pin, uint8_t sclk_pin, bool enable_pull_ups) {
    memset(radio, 0, sizeof(rda5807_t));

    radio->i2c_inst = i2c_inst;
    radio->sdio_pin = sdio_pin;
    radio->sclk_pin = sclk_pin;
    radio->enable_pull_ups = enable_pull_ups;
    radio->seek_threshold = 8;
    radio->mute = true;
    radio->softmute = true;
}

void fm_power_up(rda5807_t *radio, fm_config_t config) {
    assert(!fm_is_powered_up(radio));

    radio->config = config;
    radio->frequency_range = fm_frequency_range(config.band, config.channel_spacing);

    // configure GPIO
    i2c_inst_t *i2c_inst = radio->i2c_inst;
    gpio_set_function(radio->sdio_pin, GPIO_FUNC_I2C);
    gpio_set_function(radio->sclk_pin, GPIO_FUNC_I2C);
    if (radio->enable_pull_ups) {
        gpio_pull_up(radio->sdio_pin);
        gpio_pull_up(radio->sclk_pin);
    }

    uint16_t *regs = radio->regs;
    memset(regs, 0, sizeof(radio->regs));

    if (!fm_read_single_register(i2c_inst, regs, 0x0)) {
        panic("FM - couldn't read from I2C bus, check your wiring");
    }
    assert(regs[0] == 0x5804); // chip ID check

    // reset
    regs[0x2] = ENABLE_BIT | SOFT_RESET_BIT;
    fm_write_single_register(i2c_inst, regs, 0x2);
    sleep_ms(5);
    // clear reset bit
    regs[0x2] = ENABLE_BIT;
    fm_write_single_register(i2c_inst, regs, 0x2);
    sleep_ms(5);

    // initialize control registers
    fm_read_single_register(i2c_inst, regs, 0x3);
    fm_read_single_register(i2c_inst, regs, 0x4);
    fm_read_single_register(i2c_inst, regs, 0x5);
    fm_read_single_register(i2c_inst, regs, 0x6);
    fm_read_single_register(i2c_inst, regs, 0x7);
    fm_read_single_register(i2c_inst, regs, 0x8);

    // setup
    fm_set_bit(regs[0x2], NEW_METHOD, true);
    fm_set_bit(regs[0x2], RDS_EN, true);
    fm_set_bit(regs[0x2], BASS, radio->bass_boost);
    fm_set_bit(regs[0x2], MONO, radio->mono);
    fm_set_bit(regs[0x2], DMUTE, !radio->mute);
    fm_set_bit(regs[0x2], DHIZ, true);
    fm_set_bits(regs[0x3], CHAN, 0);
    fm_set_bit(regs[0x4], SOFTMUTE_EN, radio->softmute);
    fm_set_bit(regs[0x4], DE, config.deemphasis == FM_DEEMPHASIS_50);
    fm_set_bits(regs[0x5], VOLUME, radio->volume);
    fm_set_band_bits(regs, config.band);
    fm_set_channel_spacing_bits(regs, config.channel_spacing);
    fm_write_registers_up_to(i2c_inst, regs, 0x8);

    if (radio->frequency != 0.0f) {
        // restore frequency if waking after power down
        float frequency = radio->frequency;
        radio->frequency = 0.0f;
        fm_set_frequency_blocking(radio, frequency);
    }
}

void fm_power_down(rda5807_t *radio) {
    assert(fm_is_powered_up(radio));

    if (radio->async.task != NULL) {
        fm_async_task_cancel(radio);
    }

    uint16_t *regs = radio->regs;
    fm_set_bit(regs[0x2], ENABLE, false);
    fm_write_single_register(radio->i2c_inst, regs, 0x2);
}

bool fm_is_powered_up(rda5807_t *radio) {
    return fm_get_bit(radio->regs[0x2], ENABLE);
}

fm_config_t fm_get_config(rda5807_t *radio) {
    return radio->config;
}

fm_frequency_range_t fm_get_frequency_range(rda5807_t *radio) {
    return radio->frequency_range;
}

float fm_get_frequency(rda5807_t *radio) {
    return radio->frequency;
}

void fm_set_frequency_blocking(rda5807_t *radio, float frequency) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    if (radio->frequency == frequency) {
        return;
    }
    fm_set_frequency_async(radio, frequency);
    fm_async_progress_t progress;
    do {
        sleep_ms(TUNE_POLL_INTERVAL_MS);
        progress = fm_async_task_tick(radio);
    } while (!progress.done);
}

static fm_async_progress_t fm_set_frequency_async_task(rda5807_t *radio, bool cancel) {
    assert(radio->async.task == &fm_set_frequency_async_task);
    assert(radio->async.state == 1);

    i2c_inst_t *i2c_inst = radio->i2c_inst;
    uint16_t *regs = radio->regs;
    int result = 0;
    if (cancel) {
        result = -1;
    } else {
        fm_read_single_register(i2c_inst, regs, 0xA);
        if (!fm_get_bit(regs[0xA], STC)) {
            radio->async.resume_time = time_us_64() + TUNE_POLL_INTERVAL_MS * 1000;
            return (fm_async_progress_t){.done = false};
        }
    }

    // clear tune bit
    fm_set_bit(regs[0x3], TUNE, false);
    fm_write_single_register(i2c_inst, regs, 0x3);

    fm_read_single_register(i2c_inst, regs, 0xA);
    uint16_t channel = fm_get_bits(regs[0xA], READCHAN);
    radio->frequency = fm_channel_to_frequency(channel, radio->frequency_range);
    return (fm_async_progress_t){.done = true, result};
}

void fm_set_frequency_async(rda5807_t *radio, float frequency) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    frequency = MAX(frequency, radio->frequency_range.bottom);
    frequency = MIN(frequency, radio->frequency_range.top);
    uint16_t *regs = radio->regs;
    uint16_t channel = fm_frequency_to_channel(frequency, radio->frequency_range);
    // set channel and start tuning
    fm_set_bits(regs[0x3], CHAN, channel);
    fm_set_bit(regs[0x3], TUNE, true);
    fm_write_single_register(radio->i2c_inst, regs, 0x3);

    radio->async.task = fm_set_frequency_async_task;
    radio->async.state = 1;
    radio->async.resume_time = time_us_64() + TUNE_POLL_INTERVAL_MS * 1000;
}

uint8_t fm_get_seek_threshold(rda5807_t *radio) {
    return radio->seek_threshold;
}

void fm_set_seek_threshold(rda5807_t *radio, uint8_t seek_threshold) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    seek_threshold = MIN(seek_threshold, FM_MAX_SEEK_THRESHOLD);
    if (seek_threshold == radio->seek_threshold) {
        return;
    }
    uint16_t *regs = radio->regs;
    fm_set_bits(regs[0x5], SEEKTH, seek_threshold);
    fm_write_single_register(radio->i2c_inst, regs, 0x5);
    radio->seek_threshold = seek_threshold;
}

bool fm_seek_blocking(rda5807_t *radio, fm_seek_direction_t direction) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    fm_seek_async(radio, direction);
    fm_async_progress_t progress;
    do {
        sleep_ms(SEEK_POLL_INTERVAL_MS);
        progress = fm_async_task_tick(radio);
    } while (!progress.done);
    bool success = (progress.result == 0);
    return success;
}

static fm_async_progress_t fm_seek_async_task(rda5807_t *radio, bool cancel) {
    assert(radio->async.task == &fm_seek_async_task);
    assert(radio->async.state == 1);

    i2c_inst_t *i2c_inst = radio->i2c_inst;
    uint16_t *regs = radio->regs;
    int result = 0;
    if (cancel) {
        result = -1;
    } else {
        fm_read_single_register(i2c_inst, regs, 0xA);
        if (!fm_get_bit(regs[0xA], STC)) {
            uint16_t channel = fm_get_bits(regs[0xA], READCHAN);
            radio->frequency = fm_channel_to_frequency(channel, radio->frequency_range);
            radio->async.resume_time = time_us_64() + SEEK_POLL_INTERVAL_MS * 1000;
            return (fm_async_progress_t){.done = false};
        }

        // seek done, check seek-failed flag
        if (fm_get_bit(regs[0xA], SF)) {
            result = -1;
        }
    }

    // clear seek bit
    fm_set_bit(regs[0x2], SEEK, false);
    fm_write_single_register(i2c_inst, regs, 0x2);

    fm_read_single_register(i2c_inst, regs, 0xA);
    uint16_t channel = fm_get_bits(regs[0xA], READCHAN);
    radio->frequency = fm_channel_to_frequency(channel, radio->frequency_range);
    return (fm_async_progress_t){.done = true, result};
}

void fm_seek_async(rda5807_t *radio, fm_seek_direction_t direction) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    uint16_t *regs = radio->regs;
    fm_set_bit(regs[0x2], SKMODE, false); // wrap mode
    fm_set_bit(regs[0x2], SEEKUP, direction == FM_SEEK_UP);
    fm_set_bit(regs[0x2], SEEK, true); // start seek
    fm_write_single_register(radio->i2c_inst, regs, 0x2);

    radio->async.task = fm_seek_async_task;
    radio->async.state = 1;
    radio->async.resume_time = time_us_64() + SEEK_POLL_INTERVAL_MS * 1000;
}

bool fm_get_mute(rda5807_t *radio) {
    return radio->mute;
}

void fm_set_mute(rda5807_t *radio, bool mute) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    if (radio->mute == mute) {
        return;
    }
    uint16_t *regs = radio->regs;
    fm_set_bit(regs[0x2], DMUTE, !mute);
    fm_write_single_register(radio->i2c_inst, regs, 0x2);
    radio->mute = mute;
}

bool fm_get_softmute(rda5807_t *radio) {
    return radio->softmute;
}

void fm_set_softmute(rda5807_t *radio, bool softmute) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    if (radio->softmute == softmute) {
        return;
    }
    uint16_t *regs = radio->regs;
    fm_set_bit(regs[0x4], SOFTMUTE_EN, softmute);
    fm_write_registers_up_to(radio->i2c_inst, regs, 0x4);
    radio->softmute = softmute;
}

bool fm_get_bass_boost(rda5807_t *radio) {
    return radio->bass_boost;
}

void fm_set_bass_boost(rda5807_t *radio, bool bass_boost) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    if (radio->bass_boost == bass_boost) {
        return;
    }
    uint16_t *regs = radio->regs;
    fm_set_bit(regs[0x2], BASS, bass_boost);
    fm_write_single_register(radio->i2c_inst, regs, 0x2);
    radio->bass_boost = bass_boost;
}

bool fm_get_mono(rda5807_t *radio) {
    return radio->mono;
}

void fm_set_mono(rda5807_t *radio, bool mono) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    if (radio->mono == mono) {
        return;
    }
    uint16_t *regs = radio->regs;
    fm_set_bit(regs[0x2], MONO, mono);
    fm_write_single_register(radio->i2c_inst, regs, 0x2);
    radio->mono = mono;
}

uint8_t fm_get_volume(rda5807_t *radio) {
    return radio->volume;
}

void fm_set_volume(rda5807_t *radio, uint8_t volume) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    volume = MIN(volume, FM_MAX_VOLUME);
    if (volume == radio->volume) {
        return;
    }
    uint16_t *regs = radio->regs;
    fm_set_bits(regs[0x5], VOLUME, volume);
    fm_write_single_register(radio->i2c_inst, regs, 0x5);
    radio->volume = volume;
}

uint8_t fm_get_rssi(rda5807_t *radio) {
    assert(fm_is_powered_up(radio));

    uint16_t *regs = radio->regs;
    fm_read_single_register(radio->i2c_inst, regs, 0xB);
    uint8_t rssi = (uint8_t)fm_get_bits(regs[0xB], RSSI);
    return rssi;
}

bool fm_get_stereo_indicator(rda5807_t *radio) {
    assert(fm_is_powered_up(radio));

    uint16_t *regs = radio->regs;
    fm_read_single_register(radio->i2c_inst, regs, 0xA);
    bool stereo = fm_get_bit(regs[0xA], ST);
    return stereo;
}

bool fm_read_rds_group(rda5807_t *radio, uint16_t *blocks) {
    assert(fm_is_powered_up(radio));

    uint16_t *regs = radio->regs;
    fm_read_registers_up_to(radio->i2c_inst, regs, 0xF);
    bool rdsr = fm_get_bit(regs[0xA], RDSR);
    if (!rdsr) {
        return false; // not ready
    }
    memcpy(blocks, regs + 0xC, 4 * sizeof(uint16_t));
    return true;
}

fm_async_progress_t fm_async_task_tick(rda5807_t *radio) {
    assert(radio->async.task != NULL); // must have an async task running

    if (time_us_64() < radio->async.resume_time) {
        // skip until resume time
        return (fm_async_progress_t){.done = false};
    }
    fm_async_progress_t progress = radio->async.task(radio, false /* cancel */);
    if (progress.done) {
        radio->async = (fm_async_state_t){};
    }
    return progress;
}

void fm_async_task_cancel(rda5807_t *radio) {
    assert(radio->async.task != NULL); // must have an async task running

    radio->async.task(radio, true /* cancel */);
    radio->async = (fm_async_state_t){};
}
