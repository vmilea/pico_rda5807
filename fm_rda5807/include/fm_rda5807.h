/*
 * Copyright (c) 2021 Valentin Milea <valentin.milea@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _RDA5807_H_
#define _RDA5807_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \file fm_rda5807.h
 *
 * \brief Library for the RDA5807 FM radio chip.
 * 
 * Reference:
 * - Single-Chip Broadcast FM Radio Tuner (Rev.1.8-Aug.2014)
 */

/**
 * \brief Maximum seek threshold.
 */
#define FM_MAX_SEEK_THRESHOLD 15

/**
 * \brief Maximum volume.
 */
#define FM_MAX_VOLUME 15

typedef struct i2c_inst i2c_inst_t;

/**
 * \brief FM frequency bands.
 */
typedef enum fm_band_t
{
    FM_BAND_COMMON, // 87-108 MHz
    FM_BAND_JAPAN, // 76-91 MHz
    FM_BAND_JAPAN_WIDE, // 76-108 MHz
    FM_BAND_EAST_EUROPE, // 50-76 MHz
    FM_BAND_EAST_EUROPE_UPPER, // 65-76 MHz
} fm_band_t;

/**
 * \brief How far apart FM channels are in kHz.
 */
typedef enum fm_channel_spacing_t
{
    FM_CHANNEL_SPACING_200, // Americas, South Korea, Australia
    FM_CHANNEL_SPACING_100, // Europe, Japan
    FM_CHANNEL_SPACING_50, // Italy
    FM_CHANNEL_SPACING_25,
} fm_channel_spacing_t;

/**
 * \brief FM de-emphasis in µs.
 */
typedef enum fm_deemphasis_t
{
    FM_DEEMPHASIS_75, // Americas, South Korea
    FM_DEEMPHASIS_50, // Europe, Japan, Australia
} fm_deemphasis_t;

/**
 * \brief FM regional settings.
 */
typedef struct fm_config_t
{
    fm_band_t band;
    fm_channel_spacing_t channel_spacing;
    fm_deemphasis_t deemphasis;
} fm_config_t;

static inline fm_config_t fm_config_usa() {
    return (fm_config_t){FM_BAND_COMMON, FM_CHANNEL_SPACING_200, FM_DEEMPHASIS_75};
}

static inline fm_config_t fm_config_europe() {
    return (fm_config_t){FM_BAND_COMMON, FM_CHANNEL_SPACING_100, FM_DEEMPHASIS_50};
}

static inline fm_config_t fm_config_japan() {
    return (fm_config_t){FM_BAND_JAPAN, FM_CHANNEL_SPACING_100, FM_DEEMPHASIS_50};
}

static inline fm_config_t fm_config_japan_wide() {
    return (fm_config_t){FM_BAND_JAPAN_WIDE, FM_CHANNEL_SPACING_100, FM_DEEMPHASIS_50};
}

/**
 * \brief Frequency range in MHz corresponding to an fm_band_t.
 */
typedef struct fm_frequency_range_t
{
    float bottom; // MHz
    float top; // MHz
    float spacing; // MHz
} fm_frequency_range_t;

/**
 * \brief Direction of seek.
 */
typedef enum fm_seek_direction_t
{
    FM_SEEK_DOWN,
    FM_SEEK_UP,
} fm_seek_direction_t;

/**
 * \brief Progress of an asynchronous task.
 */
typedef struct fm_async_progress_t
{
    bool done;
    int result;
} fm_async_progress_t;

struct rda5807_t;

// private
typedef fm_async_progress_t (*fm_async_task_t)(struct rda5807_t *radio, bool cancel);

// private
typedef struct fm_async_state_t
{
    fm_async_task_t task;
    uint8_t state;
    uint64_t resume_time;
} fm_async_state_t;

/**
 * \brief FM radio.
 */
typedef struct rda5807_t
{
    i2c_inst_t *i2c_inst;
    uint8_t sdio_pin;
    uint8_t sclk_pin;
    bool enable_pull_ups;
    fm_config_t config;
    fm_frequency_range_t frequency_range;
    uint8_t seek_threshold;
    float frequency;
    uint8_t volume;
    bool mute;
    bool softmute;
    bool bass_boost;
    bool mono;
    uint16_t regs[16];
    fm_async_state_t async;
} rda5807_t;

/**
 * \brief Initialize the radio state.
 * 
 * @param radio Radio handle.
 * @param i2c_inst I2C instance.
 * @param sdio_pin SDIO pin.
 * @param sclk_pin SCLK pin.
 * @param enable_pull_ups Whether to enable the internal pull-ups on I2C pads.
 */
void fm_init(rda5807_t *radio, i2c_inst_t *i2c_inst, uint8_t sdio_pin, uint8_t sclk_pin, bool enable_pull_ups);

/**
 * \brief Power up the radio chip.
 * 
 * If waking after power down, the previous state is restored.
 * 
 * @param radio Radio handle.
 * @param config FM regional settings.
 */
void fm_power_up(rda5807_t *radio, fm_config_t config);

/**
 * \brief Power down the radio chip.
 * 
 * Puts the chip in a low power state while maintaining register configuration.
 * 
 * @param radio Radio handle.
 */
void fm_power_down(rda5807_t *radio);

/**
 * \brief Check if the radio is powered up.
 * 
 * @param radio Radio handle.
 */
bool fm_is_powered_up(rda5807_t *radio);

/**
 * \brief Get the FM regional configuration.
 * 
 * @param radio Radio handle.
 */
fm_config_t fm_get_config(rda5807_t *radio);

/**
 * \brief Get the frequency range for the configured FM band.
 * 
 * @param radio Radio handle.
 */
fm_frequency_range_t fm_get_frequency_range(rda5807_t *radio);

/**
 * \brief Get the current FM frequency.
 * 
 * @param radio Radio handle.
 * @return FM frequency in MHz.
 */
float fm_get_frequency(rda5807_t *radio);

/**
 * \brief Set the current FM frequency.
 * 
 * Tuning to a new frequency is very quick on RDA5807, at around 10ms. The non-blocking
 * fm_set_frequency_async() may be unnecessary, except for source code compatibility with
 * slower FM chips.
 * 
 * @param radio Radio handle.
 * @param frequency FM frequency in MHz.
 */
void fm_set_frequency_blocking(rda5807_t *radio, float frequency);

/**
 * \brief Set the current FM frequency without blocking.
 *
 * If canceled before completion, the tuner is stopped without restoring the original frequency.
 * 
 * May not be called while another async task is running.
 * 
 * @param radio Radio handle.
 * @param frequency FM frequency in MHz.
 *
 * @sa fm_async_task_tick(), fm_async_task_cancel()
 */
void fm_set_frequency_async(rda5807_t *radio, float frequency);

/**
 * \brief Get the seek threshold.
 * 
 * The default is 8.
 * 
 * @param radio Radio handle.
 */
uint8_t fm_get_seek_threshold(rda5807_t *radio);

/**
 * \brief Set the seek threshold.
 * 
 * Increase the seek threshold to filter out weak stations during seek.
 * 
 * @param radio Radio handle.
 * @param seek_threshold Seek threshold.
 */
void fm_set_seek_threshold(rda5807_t *radio, uint8_t seek_threshold);

/**
 * \brief Seek the next station.
 * 
 * Seeks in the given direction until a station is detected. If the frequency range limit
 * is reached, it will wrap to the other end.
 * 
 * Seeking may take a few seconds depending on how far the next station is. To avoid blocking,
 * use fm_seek_async().
 * 
 * @param radio Radio handle.
 * @param direction Seek direction.
 * @return true A strong enough station was found.
 * @return false No station was found.
 */
bool fm_seek_blocking(rda5807_t *radio, fm_seek_direction_t direction);

/**
 * \brief Seek the next radio station without blocking.
 * 
 * Seeks in the given direction until a station is detected. If the frequency range limit
 * is reached, it will wrap to the other end.
 * 
 * fm_get_frequency() may be used during the seek operation to monitor progress.
 * 
 * If canceled before completion, the tuner is stopped without restoring the original frequency.
 * 
 * May not be called while another async task is running.
 * 
 * @param radio Radio handle.
 * @param direction Seek direction.
 * 
 * @sa fm_async_task_tick(), fm_async_task_cancel()
 */
void fm_seek_async(rda5807_t *radio, fm_seek_direction_t direction);

/**
 * \brief Check whether audio is muted.
 * 
 * The audio is muted by default. After power up, you should disable mute and set the desired volume.
 * 
 * @param radio Radio handle.
 */
bool fm_get_mute(rda5807_t *radio);

/**
 * \brief Set whether audio is muted.
 * 
 * @param radio Radio handle.
 * @param mute Mute value.
 */
void fm_set_mute(rda5807_t *radio, bool mute);

/**
 * \brief Check whether softmute is enabled.
 * 
 * Softmute is enabled by default.
 * 
 * @param radio Radio handle.
 */
bool fm_get_softmute(rda5807_t *radio);

/**
 * \brief Set whether softmute is enabled.
 * 
 * Softmute reduces noise when the FM signal is too weak.
 * 
 * @param radio Radio handle.
 * @param softmute Softmute value.
 */
void fm_set_softmute(rda5807_t *radio, bool softmute);

/**
 * \brief Check whether bass boost is enabled.
 * 
 * Bass boost is disabled by default.
 * 
 * @param radio Radio handle.
 */
bool fm_get_bass_boost(rda5807_t *radio);

/**
 * \brief Set whether bass boost is enabled.
 * 
 * @param radio Radio handle.
 * @param bass_boost Bass boost value.
 */
void fm_set_bass_boost(rda5807_t *radio, bool bass_boost);

/**
 * \brief Check whether mono output is enabled.
 * 
 * The default is stereo output.
 * 
 * @param radio Radio handle.
 */
bool fm_get_mono(rda5807_t *radio);

/**
 * \brief Set whether mono output is disabled.
 * 
 * Forcing mono output may improve reception of weak stations.
 * 
 * @param radio Radio handle.
 * @param mono Mono value.
 */
void fm_set_mono(rda5807_t *radio, bool mono);

/**
 * \brief Get audio volume.
 * 
 * The default volume is 0.
 * 
 * @param radio Radio handle.
 * @return Volume in range 0-15.
 */
uint8_t fm_get_volume(rda5807_t *radio);

/**
 * \brief Set audio volume.
 * 
 * Values above 15 are clamped. Volume 0 is still hearable, use fm_set_mute() instead to
 * silence the output.
 * 
 * @param radio Radio handle.
 * @param volume Volume value in range 0-15.
 */
void fm_set_volume(rda5807_t *radio, uint8_t volume);

/**
 * \brief Get current FM signal strength.
 * 
 * After changing frequency, it's recommended to wait at least 500ms for this value to settle
 * before reading it.
 * 
 * @param radio Radio handle.
 * @return RSSI level, up to 75dBµV.
 */
uint8_t fm_get_rssi(rda5807_t *radio);

/**
 * \brief Check whether stereo signal is available.
 * 
 * If the tuned station is emitting a stereo signal, returns true regardless of mono output.
 * 
 * After changing frequency, it's recommended to wait at least 500ms for this value to settle
 * before reading it.
 * 
 * @param radio Radio handle.
 */
bool fm_get_stereo_indicator(rda5807_t *radio);

/**
 * \brief Read an RDS data group.
 * 
 * Should be called every 40ms.
 * 
 * @param radio Radio handle.
 * @param blocks Output buffer.
 * @return true RDS data ready, blocks filled.
 * @return false RDS data not yet ready.
 */
bool fm_read_rds_group(rda5807_t *radio, uint16_t *blocks);

/**
 * \brief Update the current asynchronous task.
 * 
 * Long running operations like seeking can be run asynchronously to free up the
 * CPU for other work. After calling fm_xxx_async(), the tick function must be
 * called periodically until the task is done. The tick interval is up to the user
 * (every 40ms should be fine).
 * 
 * @param radio Radio handle.
 * @return Task status.
 */
fm_async_progress_t fm_async_task_tick(rda5807_t *radio);

/**
 * \brief Abort the current asynchronous task.
 * 
 * @param radio Radio handle.
 */
void fm_async_task_cancel(rda5807_t *radio);

#ifdef __cplusplus
}
#endif

#endif // _RDA5807_H_
