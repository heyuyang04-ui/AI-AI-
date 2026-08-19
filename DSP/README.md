# DSP firmware

This directory contains the two TI C2000 firmware projects used by the power-conversion hardware:

- `PFC/`: power-factor-correction controller firmware.
- `LLC/`: LLC resonant-converter controller firmware.

Both directories are Code Composer Studio projects targeting the TMS320F2803x family. Import the project directory into CCS to build it. Generated `Debug/` and `Release/` outputs are intentionally excluded from Git.

## Third-party source notice

The `DSP2803x_common/` and `DSP2803x_headers/` directories contain Texas Instruments F2803x example/header material. Their original copyright notices are preserved. Confirm that the applicable TI license permits redistribution before making this repository public.

## Security note

`DSP2803x_CSMPasswords.asm` currently contains the TI development default (`0xFFFF` for all password words), so CSM code security is not enabled. Do not commit production device passwords if this file is later customized.
