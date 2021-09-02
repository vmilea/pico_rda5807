/*
 * Copyright (c) 2021 Valentin Milea <valentin.milea@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <rds_parser.h>
#include <string.h>

//
// misc
//

static char hex_to_char(uint8_t value) {
    assert(value < 16);

    return (value < 10 ? ('0' + value) : ('A' - 10 + value));
}

//
// rds_group
//

typedef enum
{
    RDS_GROUP_BASIC = 0x00,
    RDS_GROUP_RT = 0x02,
} rds_group_type_t;

static uint16_t rds_get_group_pi(const rds_group_t *group) {
    return group->a;
}

static rds_group_type_t rds_get_group_type(const rds_group_t *group) {
    return group->b >> 12;
}

static uint8_t rds_get_group_version(const rds_group_t *group) {
    return (group->b >> 11) & 0x01;
}

static bool rds_get_group_tp(const rds_group_t *group) {
    return ((group->b >> 10) & 0x01) != 0;
}

static uint8_t rds_get_group_pty(const rds_group_t *group) {
    return (group->b >> 5) & 0x1F;
}

//
// rds_parser_t
//

static void rds_parse_group_basic_ps(rds_parser_t *parser, const rds_group_t *group) {
    // group 0A / 0B
    size_t address = group->b & 0x3;
    size_t char_index = 2 * address;
    char ch0 = group->d >> 8;
    char ch1 = group->d & 0xFF;
    parser->ps_scratch_str[char_index] = ch0;
    parser->ps_scratch_str[char_index + 1] = ch1;

    bool finished = (address == 3);
    if (finished) {
        memcpy(parser->ps_str, parser->ps_scratch_str, 8);
    }
}

static void rds_parse_group_basic_di(rds_parser_t *parser, const rds_group_t *group) {
    // group 0A / 0B
    size_t di_bit_index = ~group->b & 0x3;
    uint8_t di_bit = (group->b >> 2) & 0x1;
    parser->di_scratch &= ~(1 << di_bit_index);
    parser->di_scratch |= di_bit << di_bit_index;

    bool finished = (di_bit_index == 0);
    if (finished) {
        parser->di = parser->di_scratch;
    }
}

#if RDS_PARSER_ALTERNATIVE_FREQUENCIES_ENABLE
static void rds_add_alt_freq(rds_parser_t *parser, uint8_t alt_freq) {
    if (alt_freq == 0 || 205 <= alt_freq) {
        return; // out of range, ignored
    }
    if (parser->alt_freq_count == sizeof(parser->alt_freq)) {
        return; // list full, ignored
    }
    for (size_t i = 0; i < parser->alt_freq_count; i++) {
        if (parser->alt_freq[i] == alt_freq) {
            return; // duplicate, ignored
        }
    }
    parser->alt_freq[parser->alt_freq_count++] = alt_freq;
}

static void rds_parse_group_basic_alt_freq(rds_parser_t *parser, const rds_group_t *group) {
    uint8_t version = rds_get_group_version(group);
    if (version != 0) {
        return;
    }
    // group 0A
    uint8_t f0 = group->c >> 8;
    uint8_t f1 = group->c & 0xFF;
    rds_add_alt_freq(parser, f0);
    rds_add_alt_freq(parser, f1);
}
#endif // RDS_PARSER_ALTERNATIVE_FREQUENCIES_ENABLE

static void rds_parse_group_basic(rds_parser_t *parser, const rds_group_t *group) {
    rds_parse_group_basic_ps(parser, group);
    rds_parse_group_basic_di(parser, group);
#if RDS_PARSER_ALTERNATIVE_FREQUENCIES_ENABLE
    rds_parse_group_basic_alt_freq(parser, group);
#endif
}

#if RDS_PARSER_RADIO_TEXT_ENABLE
static void rds_parse_group_rt(rds_parser_t *parser, const rds_group_t *group) {
    uint8_t version = rds_get_group_version(group);
    size_t address = group->b & 0xF;
    parser->rt_scratch_a_b = (group->b >> 4) & 0x1;

    char chars[4];
    size_t char_count;
    size_t char_index;
    if (version == 0) { // group 2A
        chars[0] = group->c >> 8;
        chars[1] = group->c & 0xFF;
        chars[2] = group->d >> 8;
        chars[3] = group->d & 0xFF;
        char_count = 4;
        char_index = address * 4;
    } else { // group 2B
        chars[0] = group->d >> 8;
        chars[1] = group->d & 0xFF;
        char_count = 2;
        char_index = address * 2;
    }

    bool finished = false;
    for (size_t i = 0; i < char_count; i++) {
        char ch = chars[i];
        if (ch == '\r') {
            parser->rt_scratch_str[char_index++] = '\0';
            finished = true;
            break;
        }
        parser->rt_scratch_str[char_index++] = ch;
        if (char_index == 64) {
            finished = true;
            break;
        }
    }
    if (finished) {
        memcpy(parser->rt_str, parser->rt_scratch_str, 64);
        parser->rt_a_b = parser->rt_scratch_a_b;
    }
}
#endif // RDS_PARSER_RADIO_TEXT_ENABLE

//
// public interface
//

void rds_parser_reset(rds_parser_t *parser) {
    memset(parser, 0, sizeof(rds_parser_t));
}

void rds_parser_update(rds_parser_t *parser, const rds_group_t *group) {
    parser->pi = rds_get_group_pi(group);
    parser->pty = rds_get_group_pty(group);
    parser->tp = rds_get_group_tp(group);

    switch (rds_get_group_type(group)) {
    case RDS_GROUP_BASIC:
        rds_parse_group_basic(parser, group);
        break;
#if RDS_PARSER_RADIO_TEXT_ENABLE
    case RDS_GROUP_RT:
        rds_parse_group_rt(parser, group);
        break;
#endif
    default:
        break;
    }
}

void rds_get_program_id_as_str(const rds_parser_t *parser, char *str) {
    str[0] = hex_to_char(parser->pi >> 12);
    str[1] = hex_to_char((parser->pi >> 8) & 0xF);
    str[2] = hex_to_char((parser->pi >> 4) & 0xF);
    str[3] = hex_to_char(parser->pi & 0xF);
    str[4] = '\0';
}
