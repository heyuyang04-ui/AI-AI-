---
name: esp32-openvela-development
description: Implement or review ESP32-S3-EYE firmware and openvela/NuttX applications for this repository, especially isolated UART communication with the STM32 PFC/LLC gateway. Use for ESP32 code, board configuration, pfcctl, protocol tests, and ESP32-S3-EYE integration; do not use for DSP control-loop or Qt-only work.
---

# ESP32-S3-EYE openvela development

Work inside `ESP32/` unless the user explicitly requests a coordinated protocol change in
`STM32/`, `DSP/`, or `QT/`. Read `ESP32/README.md` and the current UART contract before
changing code.

## Hardware and OS invariants

- Target ESP32-S3-EYE running openvela/NuttX, not ESP-IDF or Arduino.
- Keep USB Serial as the NSH console. Use UART1 with IO45 TX and IO46 RX for STM32.
- The STM32 side is USART6 PC7 RX and PC6 TX at 115200-8-N-1.
- Treat IO45/IO46 as boot-strapping-related pins: external circuitry must not force their
  reset levels. Use 3.3 V signaling and recommend digital isolation in the power system.
- Prefer POSIX/NuttX device APIs (`open`, `read`, `write`, `poll`, `termios`) in apps.
  Keep pin mapping and device paths configurable rather than hard-coded in application logic.

## Protocol and safety boundary

- Reuse the `A5 5A | version | command | sequence | length-le16 | payload | crc16-le`
  STM32 protocol. CRC is Modbus CRC-16 over version through payload.
- Preserve maximum payload 32 and incremental parser resynchronization. Add host tests for
  encoding, fragmented parsing, bad CRC, and any new payload decoder.
- ESP32 and the Agent may read telemetry and request fail-safe stop. They must not write DSP
  PWM, protection thresholds, raw CAN frames, fault reset, startup/ARM tokens, or bypass
  STM32 validation.
- Do not infer missing telemetry. UART v1 does not provide AC/bus voltage, temperatures,
  current reference, or independent output-enable feedback; represent these as unavailable.
- Do not implement one-shot derating by sending `enable=1`. A safe derate requires extended
  telemetry/reference fields and a persistent heartbeat service. Until both exist, reject the
  request explicitly.
- After any command, distinguish ESP32 request, STM32 ACK/REJECT, and DSP feedback. Never
  describe an ACK as proof that the physical output has changed.

## Build and verification

Keep the app self-contained under `ESP32/openvela_overlay/apps/system/` with Kconfig and
CMake integration. Put ESP32-S3-EYE configuration additions in `ESP32/configs/`.

Run the host protocol tests with the repository's documented GCC command. If an openvela
checkout and Xtensa toolchain are available, also compile the board configuration; otherwise
state that hardware/openvela compilation remains to be done and do not claim it passed.
