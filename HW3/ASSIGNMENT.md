# Recognize live joystick position and movement

Time allowed: one week. Work individually. Use the LP-MSPM0G3507 LaunchPad, BOOSTXL-EDUMKII, Code Composer Studio, and Python.

## What you will build

The LaunchPad will continuously measure joystick X and Y and send readings to your computer. Your Python application will display two predictions:

- Position: center, north, northeast, east, southeast, south, southwest, west, or northwest.
- Motion: still, horizontal sweep, vertical sweep, clockwise, or anticlockwise.

During a circle, position changes around the compass while the motion prediction identifies the rotation direction. The models run on the computer.

You will calibrate once, record one guided training round, and record one separate evaluation round. The evaluation round supplies the assignment's final results. No additional final-test recording session is required.

## 1. Set up the embedded project

The supplied starter includes the CCS project/startup, default clocks, SDK
dependencies, a dashboard frontend and HTTP interface scaffolding. **You implement
both ADCs, the sampling timer, UART setup/output and the entire acquisition/ML
backend.** No serial helper or acquisition timer is completed for you.

Disconnect USB before attaching the BoosterPack or changing jumpers. Follow the supplied setup instructions to disconnect the LaunchPad's S1 and BSL signals from PA18. Reconnect USB and confirm that the starter builds.

Configure joystick X on PA25, ADC0 channel 2, and Y on PA18, ADC1 channel 3. Approximately every 20 milliseconds, read both axes and send:

```text
sample_number,x_raw,y_raw
```

Each raw value is between 0 and 4095. Sample numbers should increase by one. Firmware does not send movement labels.

Check that readings are steady at rest, X responds most to left/right movement, and Y responds most to up/down movement. Save a short two-axis check plot.

## 2. Calibrate the physical directions

Open the dashboard. Release the joystick and record Center. Then hold fully toward the top of the board to record North, and fully toward the right to record East. Keep each position steady throughout its capture.

Use this calibration to subtract the resting center and establish the signs and scales of X and Y. After preprocessing, east should be positive X, north positive Y, and the resting center approximately zero.

## 3. Record the training round

Choose Training and start one guided round. Record each of the following once:

- Center and the eight compass directions, held steadily.
- Repeated horizontal sweeps, repeated vertical sweeps, repeated clockwise circles, and repeated anticlockwise circles.

There are 13 recordings. Each has a three-second countdown followed by six seconds of capture. For motion recordings, repeat the movement throughout the capture. For held positions, keep the joystick steady. Repeat recordings with the wrong movement or incomplete data.

Keep complete recordings together. The overlapping windows from one recording must all belong to the same data group.

## 4. Build the Python models

Write the preprocessing, training, and prediction code that connects to the supplied dashboard hooks. Use only the training round when fitting models or choosing settings.

For position, train a classifier on the held-position recordings. The input can be the mean X/Y reading over a short interval. Predict one of the nine position classes.

For the motion baseline, use two-second windows. Compute minimum, maximum, mean, and standard deviation for each axis, giving eight features. Train a 3-nearest-neighbors classifier on the five motion classes. All held-position recordings are examples of Still.

For the second motion model, add information about movement order. You may use temporal features, such as changes between successive points and signed turning, or an ordered representation of the path. Train a classifier and explain your representation. Use a method that can handle circles starting at different points.

Save the models and use the same preprocessing for training, evaluation, and live prediction.

## 5. Record and evaluate the held-out round

With model choices fixed, start a new Held-out evaluation round and record all 13 behaviors again. These are fresh recordings, separate from training.

Evaluate the saved models on this round. Report position accuracy, motion accuracy and balanced accuracy, and confusion matrices for both motion models. Balanced accuracy gives each motion class equal importance even though there are more Still windows.

Compare the two motion models, especially clockwise and anticlockwise. Discuss observed mistakes and uncertain outputs. If there are no mistakes, discuss what this small evaluation does and does not establish. Explain that overlapping windows from one recording are correlated observations.

This round is the assignment's final evaluation. Do not tune or retrain using its results and then present the same recordings as a fresh test. There is no minimum accuracy requirement.

## 6. Demonstrate continuous recognition

Keep the application running and move the joystick freely. Show held compass positions, center, both sweeps, and both rotations. Demonstrate a change between behaviors and explain the prediction delay caused by the recent-history window.

The reference system uses a two-second motion history and updates predictions about every 0.2 seconds. It displays position and motion together. New predictions come directly from the live serial stream, without manually exporting a file for each attempt.

## Submit

Submit the modified embedded project, Python model code, saved models, calibration, 13 training and 13 evaluation recordings, sensor-check plot, evaluation results, and a short live or recorded demonstration. Preserve the backend/dashboard contract in INTERFACE.md; staff will test your submission with the supplied frontend. Include a two-page report (excluding references and appendix plots), following REPORT_TEMPLATE.md, explaining the signal path, calibration, feature choices, model comparison, and limitations.

## Rubric: 100 points

| Requirement | Points |
| --- | ---: |
| Read both ADC axes | 10 |
| Stream correctly formatted readings at about 50 Hz and provide a sensor-check plot | 10 |
| Calibrate center and physical axis directions | 5 |
| Record 13 training and 13 separate evaluation trials | 10 |
| Keep recordings and their windows in the correct data groups | 5 |
| Train and use the position classifier | 10 |
| Train the eight-feature 3-nearest-neighbors motion baseline | 10 |
| Train and explain the motion model that uses order | 15 |
| Report metrics and confusion matrices | 10 |
| Explain the model comparison and limitations clearly | 5 |
| Demonstrate continuous position and motion predictions | 10 |
| Total | 100 |

## Required backend behavior and deliverables

The dashboard must support calibration, one 13-trial training round, training,
one separate 13-trial evaluation round and its final evaluation. Implement capture
countdowns/progress, repeat/cancel, persistent real recordings, training/model
loading, packet quality diagnostics and continuous prediction. Keep serial reading,
recording and training off the HTTP request path so the preview remains responsive.
After the final evaluation, freeze that result and continue live recognition.

For evaluation, use 100-sample motion windows with a 50-sample stride within
each complete trial. For live motion use 100 samples with updates every 10 new
samples. Position uses a five-sample recent interval. Measure and report actual
sampling rate. Accept recordings near 50 Hz (48–52 Hz); reject captures with
sequence gaps, corrupt data or disconnects. You may improve diagnostics without
silently filling missing values. Include all 13 behaviors once per accepted round.

Submit a folder containing Board, Machine Learning and Report. Include source
and .syscfg/projectspec files, launch instructions, saved calibration/models and
model provenance, real train/validation CSVs and manifests, axis-check and
comparison plots, metrics/confusion matrices, report and demonstration. Exclude
your .venv, CCS workspace caches and intermediate compiler outputs. Preserve
vendor licenses. Record dependencies, configuration and trial identities so staff
can reproduce evaluation and perform a fresh live demonstration.

The rubric above includes the written explanations and live behavior. Performance
is assessed through sound implementation, honest evaluation and discussion; a
perfect accuracy score is not required. Machine learning runs on the computer;
deploying a classifier on the MCU is outside the required scope.
The report is required: its technical evidence supports the implementation and
evaluation marks, and its comparison/limitations discussion earns the five points
explicitly allocated to explanation in the rubric.
