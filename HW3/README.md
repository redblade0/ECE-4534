# Joystick recognition assignment starter

Start with ASSIGNMENT.md, then Board/README.md and Machine Learning/README.md.
INTERFACE.md defines how your embedded and Python implementations connect to
the supplied dashboard. REPORT_TEMPLATE.md covers the written submission.
Extract the ZIP to a short local path, such as C:\Labs\Joystick, before importing
the CCS project. Do not build directly inside a compressed folder.

Provided: CCS project/device startup, default clocks, TI SDK dependencies and
licenses, hardware references, build/flash helpers, the complete dashboard/live
preview, HTTP transport, empty state and backend hooks.

Your work: both ADCs, sample timing, UART output, serial parsing/acquisition,
calibration, guided recording/persistence, position classifier, statistical motion
baseline, order-aware motion model, continuous inference, final evaluation/plots,
and the report. There are no completed acquisition algorithms, fitted models,
calibration values, instructor data, evaluation results or generated ADC files.

The unchanged dashboard is also the instructor's submission test interface.
The initial firmware builds but idles; the initial dashboard opens but remains
disconnected. Implement real data paths before expecting interactive collection.
