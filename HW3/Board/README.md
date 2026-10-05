# Board starter

Target: LP-MSPM0G3507 (32-bit Arm Cortex-M0+) with BOOSTXL-EDUMKII.
Read ../ASSIGNMENT.md and ../INTERFACE.md first.

## Hardware setup
Disconnect USB before changing jumpers or mounting the BoosterPack. Check the
LaunchPad guide and board-layout image in References. Remove LaunchPad J8 (S1)
and **only the BSL shunt** in J101: those connections share PA18 with joystick Y.
Keep J101 power, UART and SWD connections installed. Keep the XDS_UART routing
on J21/J22. Reattach the BoosterPack in the documented orientation and reconnect.
The joystick pushbutton, LCD and other BoosterPack sensors are outside this lab.

Pin map to verify against the board documentation:

| Signal | BoosterPack pin | MCU pin | Peripheral |
| --- | --- | --- | --- |
| Joystick X | 2 | PA25 | ADC0, input channel 2 |
| Joystick Y | 26 | PA18 | ADC1, input channel 3 |
| Debugger UART TX | LaunchPad routing | PA10 | UART0 |

## CCS import and build
Install Code Composer Studio with MSPM0 support, TI Arm Clang, and SysConfig.
The reference build used CCS 21.0.1, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.1,
and MSPM0 SDK 2.11.00.07. SDK source/metadata dependencies are included here.
In CCS, register Board/SDK in the product discovery paths, then import
firmware/joystick_starter.projectspec. If discovery requires a complete SDK,
install TI's matching full SDK and select it instead. Open the .syscfg editor
and implement the missing peripherals. Clean/rebuild after configuration changes.
CCS generates startup, linker and configuration files; do not copy the instructor's
generated ADC configuration into this project.

An optional direct build helper is supplied:

```powershell
.\build.ps1 -CcsRoot C:\ti\ccs2101\ccs
```

Use -CompilerRoot, -SysConfigRoot and -SdkRoot if your installation differs.
This helper generates firmware/generated and firmware/build. The supplied
program builds but idles without readings. Loading it replaces existing firmware.
After implementing and building, use CCS to load/run it, or explicitly run
`.\flash.ps1 -CcsRoot C:\ti\ccs2101\ccs`. Target configuration is supplied.

Implement main.c and joystick_starter.syscfg. Verify both axes and the sampling
rate before implementing the Python workflow. Save real axis-check evidence for
your report. No acquisition, sample timer, serial formatter, or ADC setup has
been implemented in this starter.
