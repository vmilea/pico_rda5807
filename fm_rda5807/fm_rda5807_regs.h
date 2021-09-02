/*
 * Copyright (c) 2021 Valentin Milea <valentin.milea@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _RDA5807_REGS_H_
#define _RDA5807_REGS_H_

#include <stdint.h>

// I2C addresses
#define RDA5807_ADDR_SEQUENTIAL    0x10
#define RDA5807_ADDR_RANDOM_ACCESS 0x11

// Register 0x0 - default: 0x5804
#define CHIPID_LSB          0 // default: 0x5804
#define CHIPID_BITS         (0xFFFF << CHIPID_LSB)

// Register 0x2 - default: 0x0000
#define ENABLE_BIT                  (1 << 0) // default: 0
#define SOFT_RESET_BIT              (1 << 1) // default: 0
#define NEW_METHOD_BIT              (1 << 2) // default: 0
#define RDS_EN_BIT                  (1 << 3) // default: 0
#define CLK_MODE_LSB                4 // default: 0b000
#define CLK_MODE_BITS               (0x7 << CLK_MODE_LSB)
#define SKMODE_BIT                  (1 << 7) // default: 0
#define SEEK_BIT                    (1 << 8) // default: 0
#define SEEKUP_BIT                  (1 << 9) // default: 0
#define RCLK_DIRECT_INPUT_MODE_BIT  (1 << 10) // default: 0
#define RCLK_NON_CALIBRATE_MODE_BIT (1 << 11) // default: 0
#define BASS_BIT                    (1 << 12) // default: 0
#define MONO_BIT                    (1 << 13) // default: 0
#define DMUTE_BIT                   (1 << 14) // default: 0
#define DHIZ_BIT                    (1 << 15) // default: 0

// Register 0x3 - default: 0x4FC0
#define SPACE_LSB           0 // 0b00
#define SPACE_BITS          (0x3 << SPACE_LSB)
#define BAND_LSB            2 // 0b00
#define BAND_BITS           (0x3 << BAND_LSB)
#define TUNE_BIT            (1 << 4) // default: 0
#define DIRECT_MODE_BIT     (1 << 5) // default: 0
#define CHAN_LSB            6 // default: 0x13F
#define CHAN_BITS           (0x3FF << CHAN_LSB)

// Register 0x4 - default: 0x0400
#define GPIO1_LSB           0 // default: b00
#define GPIO1_BITS          (0x3 << GPIO1_LSB)
#define GPIO2_LSB           2 // default: 0b00
#define GPIO2_BITS          (0x3 << GPIO2_LSB)
#define GPIO3_LSB           4 // default: 0b00
#define GPIO3_BITS          (0x3 << GPIO3_LSB)
#define I2S_ENABLE_BIT      (1 << 6) // default: 0
#define AFCD_BIT            (1 << 8) // default: 0
#define SOFTMUTE_EN_BIT     (1 << 9) // default: 0
#define RDS_FIFO_CLR_BIT    (1 << 10) // default: 1
#define DE_BIT              (1 << 11) // default: 0
#define RDS_FIFO_EN_BIT     (1 << 12) // default: 0
#define RBDS_BIT            (1 << 13) // default: 0
#define STCIEN_BIT          (1 << 14) // default: 0

// Register 0x5 - default: 0x888B
#define VOLUME_LSB          0 // default: 0b1011
#define VOLUME_BITS         (0xF << VOLUME_LSB)
#define LNA_ICSEL_LSB       4 // default: 0b00
#define LNA_ICSEL_BITS      (0x3 << LNA_ICSEL_LSB);
#define LNA_PORT_SEL_LSB    6 // default: 0b10
#define LNA_PORT_SEL_BITS   (0x3 << LNA_PORT_SEL_LSB);
#define SEEKTH_LSB          8 // default: 0b1000
#define SEEKTH_BITS         (0xF << SEEKTH_LSB)
#define SEEK_MODE_LSB       13 // default: 0b00
#define SEEK_MODE_BITS      (0x3 << SEEK_MODE_LSB)
#define INT_MODE_BIT        (1 << 15) // default: 1

// Register 0x7 - default: 0x42C6
#define FREQ_MODE_BIT         (1 << 0) // default: 0
#define SOFTBLEND_EN_BIT      (1 << 1) // default: 1
#define SEEK_TH_OLD_LSB       2 // default: 0b110001
#define SEEK_TH_OLD_BITS      (0x3F << SEEK_TH_OLD_LSB)
#define BAND_65M_50M_MODE_BIT (1 << 9) // default: 1
#define TH_SOFTBLEND_LSB      10 // default: 0b10000
#define TH_SOFTBLEND_BITS     (0x1F << TH_SOFTBLEND_LSB)

// Register 0x8 - default: 0x0000
#define FREQ_DIRECT_LSB     0 // default: 0x0000
#define FREQ_DIRECT_BITS    (0xFFFF << FREQ_DIRECT_LSB)

// Register 0xA - default: 0x013F
#define READCHAN_LSB        0 // default: 0x13F
#define READCHAN_BITS       (0x3FF >> READCHAN_LSB)
#define ST_BIT              (1 << 10) // default: 0
#define BLK_E_BIT           (1 << 11) // default: 0
#define RDSS_BIT            (1 << 12) // default: 0
#define SF_BIT              (1 << 13) // default: 0
#define STC_BIT             (1 << 14) // default: 0
#define RDSR_BIT            (1 << 15) // default: 0

// Register 0xB - default: 0x0000
#define BLERB_LSB           0
#define BLERB_BITS          (0x3 << BLERB_LSB)
#define BLERA_LSB           2
#define BLERA_BITS          (0x3 << BLERA_LSB)
#define ABCD_E_BIT          (1 << 4)
#define FM_READY_BIT        (1 << 7) // default: 0
#define FM_TRUE_BIT         (1 << 8) // default: 0
#define RSSI_LSB            9 // default: 0b0000000
#define RSSI_BITS           (0x7F << RSSI_LSB)

// Register 0xC
#define RDSA_LSB  0
#define RDSA_BITS (0xFFFF << RDSA_LSB)

// Register 0xD
#define RDSB_LSB  0
#define RDSB_BITS (0xFFFF << RDSB_LSB)

// Register 0xE
#define RDSC_LSB  0
#define RDSC_BITS (0xFFFF << RDSC_LSB)

// Register 0xF
#define RDSD_LSB  0
#define RDSD_BITS (0xFFFF << RDSD_LSB)

#endif // _RDA5807_REGS_H_
