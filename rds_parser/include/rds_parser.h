/*
 * Copyright (c) 2021 Valentin Milea <valentin.milea@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _RDS_PARSER_H_
#define _RDS_PARSER_H_

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \file rds_parser.h
 *
 * \brief RDS data parser.
 *
 * This is a basic parser for the most commonly transmitted RDS information.
 *
 */

#ifndef RDS_PARSER_RADIO_TEXT_ENABLE
#define RDS_PARSER_RADIO_TEXT_ENABLE 1
#endif

#ifndef RDS_PARSER_ALTERNATIVE_FREQUENCIES_ENABLE
#define RDS_PARSER_ALTERNATIVE_FREQUENCIES_ENABLE 1
#endif

/**
 * \brief RDS block group.
 */
typedef struct rds_group_t
{
    uint16_t a;
    uint16_t b;
    uint16_t c;
    uint16_t d;
} rds_group_t;

/**
 * \brief RDS parser.
 */
typedef struct rds_parser_t
{
    uint16_t pi; // program identification
    uint8_t pty; // program type
    bool tp; // traffic program
    bool ta; // traffic announcement
    bool ms; // music (1) / speech (0)
    uint8_t di; // decoder identification
    uint8_t di_scratch; // back-buffer for decoder identification
    char ps_str[9]; // program service name
    char ps_scratch_str[9]; // back-buffer for program service name
#if RDS_PARSER_RADIO_TEXT_ENABLE
    char rt_str[65]; // radio text
    char rt_scratch_str[65]; // back-buffer for radio text
    bool rt_a_b; // alternating radio text flag
    bool rt_scratch_a_b; // back-bufer for alternating radio text flag
#endif
#if RDS_PARSER_ALTERNATIVE_FREQUENCIES_ENABLE
    uint8_t alt_freq[25];
    uint8_t alt_freq_count;
#endif
} rds_parser_t;

/**
 * \brief Clear buffered data.
 * 
 * Should be called after changing FM frequency.
 * 
 * @param parser RDS parser.
 */
void rds_parser_reset(rds_parser_t *parser);

/**
 * \brief Process an RDS group.
 * 
 * @param parser RDS parser.
 * @param group 
 */
void rds_parser_update(rds_parser_t *parser, const rds_group_t *group);

/**
 * \brief Get the PI code
 * 
 * @param parser RDS parser
 */
static inline uint16_t rds_get_program_id(const rds_parser_t *parser) {
    return parser->pi;
}

/**
 * \brief Get the PI code as a string.
 * 
 * Writes hex value with null terminator to str.
 * 
 * @param parser RDS parser
 * @param str Output string with capacity for at least 5 chars.
 */
void rds_get_program_id_as_str(const rds_parser_t *parser, char *str);

/**
 * \brief Get the PTY code.
 * 
 * @param parser RDS parser.
 */
static inline uint8_t rds_get_program_type(const rds_parser_t *parser) {
    return parser->pty;
}

/**
 * \brief Get the dynamic PTY flag from DI.
 * 
 * @param parser RDS parser.
 */
static inline bool rds_has_dynamic_program_type(const rds_parser_t *parser) {
    return (parser->di & 0x08) != 0;
}

/**
 * \brief Get the stereo content flag from DI.
 * 
 * @param parser RDS parser.
 */
static inline bool rds_has_stereo(const rds_parser_t *parser) {
    return (parser->di & 0x01) != 0;
}

/**
 * \brief Get the MS flag.
 * 
 * @param parser RDS parser.
 */
static inline bool rds_has_music(const rds_parser_t *parser) {
    return parser->ms;
}

/**
 * \brief Get the TP flag.
 *
 * @param parser RDS parser.
 */
static inline bool rds_has_traffic_program(const rds_parser_t *parser) {
    return parser->tp;
}

/**
 * \brief Get the TA flag.
 *
 * @param parser RDS parser.
 */
static inline bool rds_has_traffic_announcement(const rds_parser_t *parser) {
    return parser->ta;
}

/**
 * \brief Get the PS string.
 * 
 * @param parser 
 */
static inline const char *rds_get_program_service_name_str(const rds_parser_t *parser) {
    return parser->ps_str;
}

#if RDS_PARSER_RADIO_TEXT_ENABLE
/**
 * \brief Get the Radio Text string.
 * 
 * See RDS code table G0 for character encoding outside of ASCII range.
 * 
 * @param parser 
 */
static inline const char *rds_get_radio_text_str(const rds_parser_t *parser) {
    return parser->rt_str;
}

/**
 * \brief Get the Radio Text A/B flag.
 * 
 * @param parser 
 */
static inline bool rds_has_alternative_radio_text(const rds_parser_t *parser) {
    return parser->rt_a_b;
}
#endif

#if RDS_PARSER_ALTERNATIVE_FREQUENCIES_ENABLE
/**
 * \brief Get the number of alternative frequencies.
 * 
 * @param parser RDS parser.
 */
static inline size_t rds_get_alternative_frequency_count(const rds_parser_t *parser) {
    return parser->alt_freq_count;
}

/**
 * \brief Get an alternative frequency.
 * 
 * Returns the raw value. Call rds_decode_alternative_frequency() to convert into MHz.
 * 
 * @param parser RDS parser.
 * @param index Frequency index.
 * @return Raw frequency value.
 */
static inline uint8_t rds_get_alternative_frequency(const rds_parser_t *parser, size_t index) {
    return parser->alt_freq[index];
}

/**
 * \brief Decode raw frequency value into MHz.
 * 
 * @param alt_freq Raw frequency value.
 */
static inline float rds_decode_alternative_frequency(uint8_t alt_freq) {
    assert(0 < alt_freq && alt_freq < 205);

    return 87.5f + alt_freq * 0.1f;
}
#endif

#ifdef __cplusplus
}
#endif

#endif // _RDS_PARSER_H_
