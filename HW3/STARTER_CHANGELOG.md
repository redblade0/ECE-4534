# Changes from the Golden Solution

This starter is a separate copy. The Golden Solution and running reference system
are unchanged. No board flashing or debugger update is performed by packaging.

Board: retain TI license, target/project structure and vendor startup/linker/driver
dependencies; replace main.c with inert acquisition/transmit hooks; reduce SysConfig
to board/default clocks. Remove both ADC instances, UART/GPIO configuration,
sampling implementation, serial helper, generated configuration, compiled firmware,
CCS workspace, complete TI adc_to_uart example and instructor sensor traces/logs.
Include pin/jumper setup directions, build/flash helpers and hardware references.

Python: retain dashboard layout, guided prompts, gating, compass/trail and live
prediction displays. Make its displayed COM port configurable. Supply only HTTP
transport, empty state, constants and command routing. Replace all acquisition,
calibration, capture/session persistence, feature extraction, fitting, prediction
and evaluation logic with high-level TODO hooks. Remove instructor recordings,
calibration values, model files, results, cached packages and full implementation
logs. Requirements and a portable launcher replace machine-specific runtime paths.

Assignment: specify calibration, 13 training trials, model training, 13 separate
evaluation trials used as the final test, then continued live recognition. Define
the frontend/backend contract and saved-evidence expectations. Add report prompts,
submission requirements and the existing 100-point rubric. No extra test session
or minimum accuracy target is required.

Staff receive a separate trusted-dashboard launcher and review checklist, outside
the student ZIP. Its structural check is a compatibility aid, not an accuracy
or academic-integrity grader. Live operation and source/report review are required.
