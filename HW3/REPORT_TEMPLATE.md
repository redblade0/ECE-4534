# Report: live joystick recognition

Name / student ID / date / source version:

Write approximately two pages, excluding references and appendix figures. Replace
the prompts with your own evidence and explanation. Refer to files in your
submission so another person can reproduce the results.

## 1. Embedded signal path and acquisition
Describe the two joystick pins/ADCs, conversion configuration, reference and
sample time, time base, paired sampling, UART format and counter policy. Include
measured rate, raw ranges and the axis-check figure. Explain how you detect missing
or corrupt samples and what happens when the board disconnects.

## 2. Calibration, data and backend
Explain how center/north/east captures establish signs/scales and map onto the
preview. Describe the state/job/session flow, background work, recording quality
checks and persistence. State the 13 training and 13 fresh evaluation trial IDs.
Explain why windows from one trial must stay together and identify which
preprocessing choices were fitted using training data only.

## 3. Models and comparison
Explain position inputs/classifier, the eight-feature scaled 3-NN motion baseline,
and your order-aware representation/classifier. Include feature definitions and
hyperparameters, window lengths, strides and live update timing. Explain how your
representation distinguishes clockwise from anticlockwise at different starting
points. Cite any external implementation or reference used.

## 4. Final held-out results and limitations
Report position accuracy; both motion models' accuracy and balanced accuracy;
confusion matrices with label order and counts; comparison plot; optional
abstention coverage. Discuss circle direction, transition delays and observed
errors. If accuracy is perfect, explain the limitations of one operator, one
short round per split and correlated windows. Do not claim independent samples
or generalization to new users from this experiment alone.

## 5. Reproduction and live demonstration
Give launch/build commands, dependency versions, artifact locations and the model
fingerprint used for evaluation. Link a short demonstration of all held positions,
sweeps, both rotations and a transition while position and motion appear together.
Describe anything incomplete and any known limitation honestly.

Appendix: axis-check/comparison/confusion plots and additional evidence if needed.
