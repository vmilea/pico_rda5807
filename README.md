# RDA5807 FM radio library for the Raspberry Pi Pico

This library provides a convenient interface to the RDA5807 FM radio chip from RDA Microelectronics. It targets the Raspberry Pi Pico and similar RP2040 based boards using the C/C++ SDK.

![RDA5807M_400px](https://user-images.githubusercontent.com/278476/131880973-bcd468be-c54e-4abb-b385-dbdfcb7c8bb3.png)

## Overview

Main controls:

- FM band (87-108 / 76-91 / 50-76 MHz)
- channel spacing and de-emphasis
- seek sensitivity
- volume
- softmute (mutes noisy frequencies)
- mono / stereo output
- bass-boost

Features:

- tune / seek the next station without blocking the CPU
- monitor signal strength and stereo signal
- RDS - decode station name, radio-text, and alternative frequencies

## Example

A test program is included. To interact with it, establish a serial connection through USB or UART.

```
RDA5807 - test program
======================
- =   Volume down / up
1-9   Station presets
{ }   Frequency down / up
[ ]   Seek down / up
<     Reduce seek threshold
>     Increase seek threshold
0     Toggle mute
f     Toggle softmute
m     Toggle mono
b     Toggle bass boost
i     Print station info
r     Print RDS info
x     Power down
```

### Building

Follow the instructions in [Getting started with Raspberry Pi Pico](https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf) to setup your build environment. Then:

- clone repo
- `mkdir build`, `cd build`, `cmake ../`, `make`
- copy `fm_example.uf2` to Raspberry Pico

### Wiring

Communication is done through I2C. The default pins are:

| RDA5807 pin | Raspberry Pi Pico pin |
| ----------- | --------------------- |
| SDA         | GP4                   |
| SCL         | GP5                   |

When powering the FM chip straight from Pico 3v3 OUT there is a fair amount of noise, so a separate power supply is recommended.

## Links

[pico_si470x](https://github.com/vmilea/pico_si470x) is a similar library for the Si4702 / Si4703 FM radio chips.

## Authors

Valentin Milea <valentin.milea@gmail.com>
