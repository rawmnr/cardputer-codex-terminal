# Hardware

## Platform

Target hardware: M5Stack Cardputer ADV, based on the ESP32-S3.

## Relevant Capabilities

- ESP32-S3 SoC with two Xtensa LX7 cores.
- Built-in Wi-Fi.
- ST7789V2 TFT display.
- 56-key physical keyboard.
- PDM MEMS digital microphone.
- Built-in speaker.
- Internal battery plus expansion base.

## Functional Pinout

| Peripheral | Interface | Pins | Project Use |
| --- | --- | --- | --- |
| SPM1423 microphone | I2S/PDM input | DAT GPIO 46, CLK GPIO 43 | Push-to-talk capture |
| NS4168 speaker | I2S output | BCLK 41, SDATA 42, LRCLK 43 | Alerts and notifications |
| ST7789V2 display | SPI | CS 37, SCK 36, DAT 35, RST 33, RS 34, BL 38 | Visual terminal |
| Keyboard | GPIO matrix | internal | Text commands and approvals |

## Notes

- GPIO 43 is shared between microphone clock and speaker LRCLK.
- Audio capture should favor DMA and circular buffers.
- PSRAM should be considered for audio and display buffers.
- Battery life depends heavily on Wi-Fi, display, and network encryption usage.
