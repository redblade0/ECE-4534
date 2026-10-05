# Fixed integration contract — version 1

The frontend, vocabulary and protocol are supplied so students and staff share
one test interface. This document specifies behavior, not its implementation.

## Board to computer

115200 baud, 8 data bits, no parity, 1 stop bit. Approximately 50 sample pairs/s.
Each packet is ASCII `sample_number,x_raw,y_raw` followed by CRLF. No header,
labels or debug text on this stream. Counter is an unsigned 32-bit sequence;
both raw values are integers 0–4095. Document reset and wrap handling. Expose
malformed packets, sequence gaps, stale data and measured rate rather than
accepting invented readings. A valid pair comes from two actual ADC conversions.

## Public Python interface

Module backend.py exports `Backend(port, data_dir)` with `start()`, `close()`,
`snapshot()` and `action(command, payload)`. start begins background work;
close releases it. snapshot returns a coherent JSON-serializable dictionary
without blocking for acquisition/training. action validates and starts requested
jobs promptly. Reject unavailable/invalid actions with ValueError or RuntimeError.
The supplied server uses GET /, GET /api/state and POST /api/action. Successful
actions return {"ok": true}; failures return {"error": "explanation"} with a
non-2xx status. The request payload includes command alongside its arguments.

## Dashboard state

Keep every top-level field returned by contract.empty_state. Null means absent;
lists and dictionaries must have the indicated shapes. Numbers must be finite.

| Field | Meaning and shape |
| --- | --- |
| connected | bool: stream is currently fresh; stale/disconnected is false |
| port | string: requested board UART port |
| raw | null or [x_raw, y_raw] for the latest real pair |
| path | list of recent normalized [x,y] pairs, oldest first; +X east, +Y north |
| calibration | object with saved center/north/east [mean_raw_x,mean_raw_y] captures |
| calibrated | bool: all three captures form a usable calibration |
| result | object with position and motion string labels, optional position_score/motion_score in [0,1] |
| job | null or {kind, label, count, received, countdown} |
| session | null or {split, saved, total, completed, next} |
| model | null or {trials: integer}; optional additional metadata |
| message | string: status, saved-result summary or actionable error |
| good, bad, gaps | nonnegative integer stream diagnostics |
| rate | measured Hz, finite float; 0 while unavailable |
| busy | bool: background training/evaluation is working |
| sealed | bool: the separate evaluation has been completed and fixed |

job.kind is calibration or trial; label is a pose/behavior. count is 100 for
calibration or 300 for a trial; received counts actual accepted readings, not
elapsed UI polls. countdown is the remaining preparation time in seconds,
zero during capture. session.split is train or validation; total is 13, saved
counts accepted trials, completed is bool, next is the next behavior label or
null after completion. The frontend reads this to select prompts and enable
controls. Do not mark connected/calibrated/completed artificially to bypass them.

Position labels: center, north, northeast, east, southeast, south, southwest,
west, northwest. Motion labels: still, horizontal, vertical, clockwise,
anticlockwise. Held-position trials are motion=still. Live status labels may be
not trained, warming up, no data, disconnected or uncertain. Scores are model
scores, not measured accuracy. Do not apply a compulsory confidence cutoff;
if you add abstention, report its coverage separately from classifier accuracy.

## Actions and lifecycle

| command | Arguments | Required behavior |
| --- | --- | --- |
| calibrate | label: center/north/east | 3-second countdown, 100-sample real capture, save and update state |
| session | split: train/validation, rounds: 1 | Create one 13-behavior round; evaluation requires a trained model |
| record | none | Countdown then accept next 300-sample capture and advance prompt |
| repeat | none | Withdraw most recent accepted trial of current round and recapture its behavior |
| cancel | none | Cancel active capture/countdown; accept no partial trial |
| train | none | Require complete training round; fit/persist models, enable live predictions |
| evaluate | split: validation | Require complete distinct evaluation round; use frozen models, save result, seal |

Round order: the nine position labels in the order above, then horizontal,
vertical, clockwise and anticlockwise. Reject competing jobs and invalid lifecycle
transitions. Reject recordings with timing outside 48–52 Hz, malformed packets,
sequence gaps or disconnection during capture. Return to a recapturable prompt.
Keep incomplete/withdrawn recordings out of accepted training/evaluation inputs.

Freeze calibration and the model choices used for evaluation. Track a model
fingerprint and the calibration used. Training and evaluation trial identities
must be disjoint. No third round is required. Once evaluation succeeds, sealed
locks collection/retraining in the frontend; leave acquisition/live prediction
running. On restart, restore saved calibration/model/results and their provenance
without treating old samples as fresh live history. Explain in the report how to
start a genuinely new experiment without overwriting the final evidence.
Reject recalibration once a model exists in the current experiment, even if a
frontend button permits the request. Use a new artifact workspace for a new
experiment. Backend validation, rather than frontend gating alone, protects data.

## Saved artifacts beneath data_dir (default artifacts/)

* calibration.json: center, north, east raw means and calibration metadata.
* data/train/<session_id>/ and data/validation/<session_id>/: each manifest.json
  records split, unique session/trial IDs, behavior, position/motion label,
  accepted/withdrawn status, CSV filename, actual sample count, rate and quality
  diagnostics. Retain evidence of rejected/repeated captures separately.
* Each accepted trial CSV: sample_number,x_raw,y_raw,host_time_s. host_time_s is
  a consistent host timestamp basis within the capture; document the clock.
  The CSV has one header row and exactly 300 actual sample rows.
* models/: saved position, baseline motion and ordered motion models, with
  preprocessing/calibration, label order, feature definitions, dependency
  versions and training trial IDs. File format is your choice; document loading.
* results/evaluation.json: position accuracy plus baseline and ordered motion
  accuracy, balanced accuracy, label order, confusion matrices, observation
  counts, train/evaluation trial IDs, model fingerprint and any abstention coverage.
* results/comparison.png: compare the two motion models on the same held-out
  windows. Show per-class results or confusion matrices, labels and denominators.
* results/axis-check.png: real two-axis sensor verification.

Document your manifest and model formats so staff can reproduce evaluation.
Do not redistribute instructor traces, saved calibration or trained models.
The instructor's launcher can set a different data_dir; respect that argument.
