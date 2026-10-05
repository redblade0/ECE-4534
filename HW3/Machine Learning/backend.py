from recognition import (
    train_models,
    predict_live,
    evaluate_models,
    save_evaluation_plots,
)


from copy import deepcopy
from pathlib import Path
from threading import RLock, Thread
from collections import deque
import csv
import json
import math
import time
import uuid
import joblib

from contract import (
    empty_state,
    BEHAVIORS,
    CALIBRATION_SAMPLES,
    TRIAL_SAMPLES,
)
from acquisition import SerialReader


class Backend:
    def __init__(self, port, data_dir):
        self.port = port
        self.data_dir = Path(data_dir)

        self.lock = RLock()
        self.state = empty_state(port)

        self.reader = None
        self.worker = None

        # Live prediction is deliberately decoupled from serial acquisition.
        # The serial callback only stores samples and queues the newest
        # prediction request; the prediction worker does the ML work outside
        # the acquisition lock.
        self.prediction_thread = None
        self.prediction_event = __import__('threading').Event()
        self.prediction_stop_event = __import__('threading').Event()
        self.prediction_pending = None
        self.prediction_request_id = 0

        self.samples = deque(maxlen=200)
        self.capture_queue = deque()
        self.last_sample_time = None

        self.calibration_samples = {
            'center': [],
            'north': [],
            'east': [],
        }

        self.calibration = None

        self.session_split = None
        self.session_id = None
        self.session_labels = []
        self.session_index = 0
        self.session_saved = 0

        self.normalized_samples = deque(maxlen=100)
        self.live_prediction_counter = 0

        self.trials = []
        self.model_bundle = None

        self.capture_cancel = False

        self.stream_bad = False
        self.stream_gap = False
        self.stream_reset = False

    def start(self):
        self.data_dir.mkdir(parents=True, exist_ok=True)

        self._load_calibration()

        self.reader = SerialReader(
            self.port,
            self._on_sample,
            self._on_status
        )

        self.prediction_stop_event.clear()
        self.prediction_thread = Thread(
            target=self._prediction_loop,
            name='live-prediction',
            daemon=True,
        )
        self.prediction_thread.start()

        self.reader.start()

    def close(self):
        self.capture_cancel = True
        self.prediction_stop_event.set()
        self.prediction_event.set()

        if self.reader is not None:
            self.reader.close()
            self.reader = None

        if self.worker is not None and self.worker.is_alive():
            self.worker.join(timeout=2)

        self.worker = None

        if (
            self.prediction_thread is not None
            and self.prediction_thread.is_alive()
        ):
            self.prediction_thread.join(timeout=1.0)

        self.prediction_thread = None

    def snapshot(self):
        with self.lock:
            state = deepcopy(self.state)

        if (
            self.last_sample_time is not None
            and time.monotonic() - self.last_sample_time > 1.0
        ):
            state['connected'] = False
            state['message'] = 'No joystick data received'
            state['result'] = {
                'position': 'stale',
                'motion': 'stale',
            }

        return state

    def action(self, command, payload):
        handlers = {
            'calibrate': self.calibrate,
            'session': self.begin_session,
            'record': self.record,
            'repeat': self.repeat,
            'cancel': self.cancel,
            'train': self.train,
            'evaluate': self.evaluate,
        }

        if command not in handlers:
            raise ValueError('Unknown dashboard command')

        handlers[command](payload)

    def _start_worker(self, target, *args):
        if self.worker is not None and self.worker.is_alive():
            raise RuntimeError('Another operation is already running')

        self.worker = Thread(
            target=target,
            args=args,
            daemon=True
        )

        self.worker.start()

    def _on_sample(
        self,
        sample_number,
        x,
        y,
        timestamp,
        rate,
        gap,
        reset
    ):
        """
        Keep the serial callback lightweight.

        Acquisition must never wait for sklearn or for browser polling.
        """

        normalized = self._normalize(x, y)

        with self.lock:
            sample = {
                'sample_number': sample_number,
                'x': x,
                'y': y,
                'host_time_s': timestamp,
                'rate': rate,
                'gap': gap,
                'reset': reset,
            }

            self.samples.append(sample)

            # Only queue samples while a real capture job is active. This
            # prevents an unbounded queue during normal live operation.
            if self.state['job'] is not None:
                self.capture_queue.append(sample)

            self.last_sample_time = timestamp

            self.state['connected'] = True
            self.state['raw'] = [x, y]
            self.state['rate'] = rate
            self.state['good'] = self.state.get('good', 0) + 1

            if gap:
                self.stream_gap = True
                self.state['gaps'] = self.state.get('gaps', 0) + 1

            if reset:
                self.stream_reset = True

            self.normalized_samples.append(normalized)

            self.state['path'].append(normalized)
            if len(self.state['path']) > 100:
                self.state['path'].pop(0)

            if self.state['job'] is not None:
                pass
            elif self.state['calibrated']:
                pass
            else:
                self.state['message'] = 'Receiving joystick data'

            if self.model_bundle is not None:
                self.live_prediction_counter += 1

                # Assignment requirement: trigger a new prediction every
                # 10 incoming samples (~0.2 s at 50 Hz).
                if self.live_prediction_counter >= 10:
                    self.live_prediction_counter = 0

                    self.prediction_request_id += 1
                    self.prediction_pending = (
                        self.prediction_request_id,
                        list(self.normalized_samples),
                        self.model_bundle,
                    )
                    self.prediction_event.set()

    def _prediction_loop(self):
        """
        Run live ML outside the serial reader and backend state lock.

        Pending work is coalesced so an unexpectedly slow prediction cannot
        build a multi-second backlog of stale predictions.
        """
        while not self.prediction_stop_event.is_set():
            self.prediction_event.wait(timeout=0.25)

            if self.prediction_stop_event.is_set():
                break

            with self.lock:
                pending = self.prediction_pending
                self.prediction_pending = None
                self.prediction_event.clear()

            if pending is None:
                continue

            request_id, history, bundle = pending

            try:
                prediction = predict_live(history, bundle)
            except Exception as error:
                with self.lock:
                    if request_id == self.prediction_request_id:
                        self.state['message'] = f'Live prediction error: {error}'
                continue

            with self.lock:
                # Never publish a prediction older than a newer queued request.
                if request_id == self.prediction_request_id:
                    self.state['result'] = prediction

    def _on_status(self, status):
        with self.lock:
            if 'connected' in status:
                self.state['connected'] = status['connected']

                if not status['connected']:
                    self.state['result'] = {
                        'position': 'stale',
                        'motion': 'stale',
                    }

            if 'message' in status:
                self.state['message'] = status['message']

            if 'good' in status:
                self.state['good'] = status['good']

            if 'bad' in status:
                self.state['bad'] = status['bad']

            if 'gaps' in status:
                self.state['gaps'] = status['gaps']

            if 'rate' in status:
                self.state['rate'] = status['rate']

    def calibrate(self, payload):
        label = payload.get('label')

        if label not in ('center', 'north', 'east'):
            raise ValueError('Calibration must be center, north, or east')

        with self.lock:
            if self.state['model'] is not None:
                raise RuntimeError(
                    'Calibration is locked after training. Start a new experiment.'
                )

            if not self.state['connected']:
                raise RuntimeError('Board is not connected')

            if self.state['job'] is not None:
                raise RuntimeError('Another operation is already running')

            self.state['message'] = f'Prepare for {label} calibration'

        self.capture_cancel = False
        self._start_worker(self._calibration_worker, label)

    def _calibration_worker(self, label):
        try:
            self._countdown('calibration', label, CALIBRATION_SAMPLES)

            readings = self._capture_samples(
                CALIBRATION_SAMPLES,
                'calibration',
                label
            )

            xs = [sample['x'] for sample in readings]
            ys = [sample['y'] for sample in readings]

            mean_x = sum(xs) / len(xs)
            mean_y = sum(ys) / len(ys)

            with self.lock:
                self.calibration_samples[label] = readings

                self.state['calibration'][label] = [
                    mean_x,
                    mean_y
                ]

            self._finish_calibration()

        except Exception as error:
            with self.lock:
                self.state['job'] = None
                self.state['message'] = str(error)

        finally:
            self.capture_cancel = False

    def _finish_calibration(self):
        with self.lock:
            calibration = dict(self.state['calibration'])

            required = ('center', 'north', 'east')

            if not all(calibration.get(label) is not None for label in required):
                self._save_calibration()

                self.state['job'] = None
                self.state['message'] = 'Calibration point saved'
                return

            center = calibration['center']
            north = calibration['north']
            east = calibration['east']

        cx, cy = center
        nx, ny = north
        ex, ey = east

        north_dx = nx - cx
        north_dy = ny - cy

        east_dx = ex - cx
        east_dy = ey - cy

        north_length = math.hypot(north_dx, north_dy)
        east_length = math.hypot(east_dx, east_dy)

        if north_length < 100:
            with self.lock:
                self.state['job'] = None
            raise RuntimeError(
                'Calibration rejected: North range is too small'
            )

        if east_length < 100:
            with self.lock:
                self.state['job'] = None
            raise RuntimeError(
                'Calibration rejected: East range is too small'
            )

        cross = (
            east_dx * north_dy
            - east_dy * north_dx
        )

        if abs(cross) < 10000:
            with self.lock:
                self.state['job'] = None
            raise RuntimeError(
                'Calibration rejected: axes are not sufficiently independent'
            )

        new_calibration = {
            'center_x': cx,
            'center_y': cy,
            'north_x': nx,
            'north_y': ny,
            'east_x': ex,
            'east_y': ey,
            'north_length': north_length,
            'east_length': east_length,
        }

        with self.lock:
            self.calibration = new_calibration

            # Explicitly store the three saved calibration points
            # in the exact shape required by the dashboard contract.
            self.state['calibration'] = {
                'center': [cx, cy],
                'north': [nx, ny],
                'east': [ex, ey],
            }

            # This is the flag the frontend uses for "calibrated".
            self.state['calibrated'] = True

            self.state['job'] = None
            self.state['message'] = 'Calibration complete'

        # Save AFTER state has been updated.
        self._save_calibration()



    def _countdown(self, kind, label, count):
        for remaining in range(3, 0, -1):
            if self.capture_cancel:
                raise RuntimeError('Capture cancelled')

            with self.lock:
                if not self.state['connected']:
                    raise RuntimeError('Board disconnected')

                self.state['job'] = {
                    'kind': kind,
                    'label': label,
                    'count': count,
                    'received': 0,
                    'countdown': remaining,
                }

            time.sleep(1)

    def _capture_samples(self, count, kind, label):
        readings = []
        last_number = None

        with self.lock:
            # Remove samples that arrived before this recording started.
            self.capture_queue.clear()

            start_bad = self.state['bad']
            start_gaps = self.state['gaps']

        while len(readings) < count:
            if self.capture_cancel:
                raise RuntimeError('Capture cancelled')

            sample = None

            with self.lock:
                if (
                    not self.state['connected']
                    or self.last_sample_time is None
                    or time.monotonic() - self.last_sample_time > 1.0
                ):
                    raise RuntimeError('Board disconnected or stream timed out')

                if self.state['bad'] != start_bad:
                    raise RuntimeError(
                        'Capture rejected: malformed packet detected'
                    )

                if self.state['gaps'] != start_gaps:
                    raise RuntimeError(
                        'Capture rejected: sample sequence gap detected'
                    )

                if self.capture_queue:
                    sample = self.capture_queue.popleft()

                if sample is not None:
                    if sample['gap'] or sample['reset']:
                        raise RuntimeError(
                            'Capture rejected: invalid sample sequence'
                        )

                    if last_number is not None:
                        expected = (last_number + 1) & 0xFFFFFFFF

                        if sample['sample_number'] != expected:
                            raise RuntimeError(
                                'Capture rejected: sample sequence gap detected'
                            )

                    readings.append(sample)
                    last_number = sample['sample_number']

                    self.state['job'] = {
                        'kind': kind,
                        'label': label,
                        'count': count,
                        'received': len(readings),
                        'countdown': 0,
                    }

            if sample is None:
                time.sleep(0.002)

        # Validate the rate of the completed capture as a whole.
        timestamps = [
            sample['host_time_s']
            for sample in readings
        ]

        if len(timestamps) >= 2:
            elapsed = timestamps[-1] - timestamps[0]

            if elapsed <= 0:
                raise RuntimeError(
                    'Capture rejected: invalid timing data'
                )

            average_rate = (
                (len(timestamps) - 1) / elapsed
            )

            if average_rate < 48 or average_rate > 52:
                raise RuntimeError(
                    f'Capture rejected: average sample rate '
                    f'{average_rate:.1f} Hz outside 48–52 Hz'
                )

        return readings



    def _save_calibration(self):
        path = self.data_dir / 'calibration.json'

        with self.lock:
            data = {
                'center': self.state['calibration'].get('center'),
                'north': self.state['calibration'].get('north'),
                'east': self.state['calibration'].get('east'),
                'calibrated': self.state['calibrated'],
                'saved_at': time.time(),
            }

        path.write_text(
            json.dumps(data, indent=2),
            encoding='utf-8'
        )


    def _load_calibration(self):
        path = self.data_dir / 'calibration.json'

        if not path.exists():
            return

        try:
            data = json.loads(
                path.read_text(encoding='utf-8')
            )

            saved = data.get('calibration', data)

            def point(label):
                value = saved.get(label)

                if isinstance(value, dict):
                    return [
                        float(value['x']),
                        float(value['y'])
                    ]

                if isinstance(value, (list, tuple)) and len(value) == 2:
                    return [
                        float(value[0]),
                        float(value[1])
                    ]

                return None

            center = point('center')
            north = point('north')
            east = point('east')

            if center is None or north is None or east is None:
                return

            cx, cy = center
            nx, ny = north
            ex, ey = east

            north_dx = nx - cx
            north_dy = ny - cy

            east_dx = ex - cx
            east_dy = ey - cy

            north_length = math.hypot(
                north_dx,
                north_dy
            )

            east_length = math.hypot(
                east_dx,
                east_dy
            )

            if north_length < 100:
                return

            if east_length < 100:
                return

            cross = (
                east_dx * north_dy
                - east_dy * north_dx
            )

            if abs(cross) < 10000:
                return

            self.calibration = {
                'center_x': cx,
                'center_y': cy,
                'north_x': nx,
                'north_y': ny,
                'east_x': ex,
                'east_y': ey,
                'north_length': north_length,
                'east_length': east_length,
            }

            # IMPORTANT: keep the frontend state in the shape
            # expected by the dashboard.
            self.state['calibration'] = {
                'center': center,
                'north': north,
                'east': east,
            }

            self.state['calibrated'] = True
            self.state['message'] = 'Calibration loaded'

        except Exception:
            return


    def _normalize(self, x, y):
        if self.calibration is None:
            return [0, 0]

        c = self.calibration

        dx = x - c['center_x']
        dy = y - c['center_y']

        north_x = c['north_x'] - c['center_x']
        north_y = c['north_y'] - c['center_y']

        east_x = c['east_x'] - c['center_x']
        east_y = c['east_y'] - c['center_y']

        determinant = (
            east_x * north_y
            - east_y * north_x
        )

        if abs(determinant) < 1e-9:
            return [0, 0]

        east_component = (
            dx * north_y
            - dy * north_x
        ) / determinant

        north_component = (
            east_x * dy
            - east_y * dx
        ) / determinant

        east_component = max(
            -1.15,
            min(1.15, east_component)
        )

        north_component = max(
            -1.15,
            min(1.15, north_component)
        )

        return [
            east_component,
            north_component
        ]

    def begin_session(self, payload):
        split = payload.get('split')

        if split not in ('train', 'validation'):
            raise ValueError('Split must be train or validation')

        if payload.get('rounds') != 1:
            raise ValueError('Exactly one round is required')

        with self.lock:
            if not self.state['calibrated']:
                raise RuntimeError('Calibrate the joystick first')

            if not self.state['connected']:
                raise RuntimeError('Board is not connected')

            if self.state['job'] is not None:
                raise RuntimeError('Another operation is already running')

            if self.state['session'] is not None:
                raise RuntimeError('A session is already active')

            if split == 'validation' and self.state['model'] is None:
                raise RuntimeError(
                    'A trained model is required before validation'
                )

            self.session_split = split
            self.session_id = (
                f'{split}_{time.strftime("%Y%m%d_%H%M%S")}_'
                f'{uuid.uuid4().hex[:8]}'
            )

            self.session_labels = list(BEHAVIORS)
            self.session_index = 0
            self.session_saved = 0
            self.trials = []

            self.state['session'] = {
                'split': split,
                'saved': 0,
                'total': len(BEHAVIORS),
                'completed': False,
                'next': BEHAVIORS[0],
            }

            session_dir = (
                self.data_dir
                / 'data'
                / split
                / self.session_id
            )

            session_dir.mkdir(
                parents=True,
                exist_ok=True
            )

            self._write_manifest(session_dir)

            self.state['message'] = (
                'Training round started'
                if split == 'train'
                else 'Validation round started'
            )

    def record(self, payload):
        with self.lock:
            session = self.state['session']

            if session is None:
                raise RuntimeError('Start a collection session first')

            if session['completed']:
                raise RuntimeError('Session is already complete')

            if self.state['job'] is not None:
                raise RuntimeError('Another operation is already running')

            if not self.state['connected']:
                raise RuntimeError('Board is not connected')

            label = session['next']

        self.capture_cancel = False

        self._start_worker(
            self._record_worker,
            label
        )

    def _record_worker(self, label):
        try:
            self._countdown(
                'trial',
                label,
                TRIAL_SAMPLES
            )

            readings = self._capture_samples(
                TRIAL_SAMPLES,
                'trial',
                label
            )

            self._accept_trial(
                label,
                readings
            )

        except Exception as error:
            with self.lock:
                self.state['job'] = None
                self.state['message'] = str(error)

        finally:
            self.capture_cancel = False

    def _accept_trial(self, label, readings):
        split = self.session_split
        session_id = self.session_id

        session_dir = (
            self.data_dir
            / 'data'
            / split
            / session_id
        )

        trial_id = (
            f'{session_id}_{self.session_saved + 1:02d}_'
            f'{uuid.uuid4().hex[:8]}'
        )

        filename = f'{trial_id}.csv'
        csv_path = session_dir / filename

        with csv_path.open(
            'w',
            newline='',
            encoding='utf-8'
        ) as file:
            writer = csv.writer(file)

            writer.writerow([
                'sample_number',
                'x_raw',
                'y_raw',
                'host_time_s',
            ])

            for sample in readings:
                writer.writerow([
                    sample['sample_number'],
                    sample['x'],
                    sample['y'],
                    sample['host_time_s'],
                ])

        rates = [
            sample['rate']
            for sample in readings
            if sample['rate'] > 0
        ]

        average_rate = (
            sum(rates) / len(rates)
            if rates
            else 0
        )

        trial = {
            'trial_id': trial_id,
            'split': split,
            'session_id': session_id,
            'behavior': label,
            'position': (
                label
                if label not in (
                    'horizontal',
                    'vertical',
                    'clockwise',
                    'anticlockwise'
                )
                else 'center'
            ),
            'motion': (
                'still'
                if label not in (
                    'horizontal',
                    'vertical',
                    'clockwise',
                    'anticlockwise'
                )
                else label
            ),
            'accepted': True,
            'withdrawn': False,
            'csv': filename,
            'samples': len(readings),
            'rate': average_rate,
            'bad': 0,
            'gaps': 0,
        }

        self.trials.append(trial)
        self.session_saved += 1
        self.session_index += 1

        self._write_manifest(session_dir)

        with self.lock:
            if self.session_index >= len(self.session_labels):
                self.state['session'] = {
                    'split': split,
                    'saved': self.session_saved,
                    'total': len(BEHAVIORS),
                    'completed': True,
                    'next': None,
                }

                self.state['message'] = (
                    'Training collection complete'
                    if split == 'train'
                    else 'Validation collection complete'
                )

            else:
                next_label = self.session_labels[
                    self.session_index
                ]

                self.state['session'] = {
                    'split': split,
                    'saved': self.session_saved,
                    'total': len(BEHAVIORS),
                    'completed': False,
                    'next': next_label,
                }

                self.state['message'] = (
                    f'{label.capitalize()} saved'
                )

            self.state['job'] = None

    def _write_manifest(self, session_dir):
        manifest_path = session_dir / 'manifest.json'

        manifest = {
            'split': self.session_split,
            'session_id': self.session_id,
            'trials': self.trials,
        }

        manifest_path.write_text(
            json.dumps(
                manifest,
                indent=2
            ),
            encoding='utf-8'
        )

    def repeat(self, payload):
        with self.lock:
            if self.state['job'] is not None:
                raise RuntimeError('Another operation is already running')

            if self.state['session'] is None:
                raise RuntimeError('No active session')

            if not self.trials:
                raise RuntimeError('No accepted trial to repeat')

            if self.state['session']['completed']:
                raise RuntimeError(
                    'The round is complete; repeat is no longer available'
                )

            previous = self.trials[-1]

            if not previous['accepted']:
                raise RuntimeError('No accepted trial to repeat')

            previous['accepted'] = False
            previous['withdrawn'] = True

            trial_number = len(self.trials)

            session_dir = (
                self.data_dir
                / 'data'
                / self.session_split
                / self.session_id
            )

            self._write_manifest(session_dir)

            self.session_index -= 1
            self.session_saved -= 1

            label = previous['behavior']

            self.state['session'] = {
                'split': self.session_split,
                'saved': self.session_saved,
                'total': len(BEHAVIORS),
                'completed': False,
                'next': label,
            }

            self.state['message'] = (
                f'Previous {label} trial withdrawn. '
                f'Record it again.'
            )

    def cancel(self, payload):
        with self.lock:
            if self.state['job'] is None:
                return

            self.capture_cancel = True
            self.state['job'] = None
            self.state['message'] = 'Capture cancelled'

    def train(self, payload):
        with self.lock:
            if not self.state['calibrated']:
                raise RuntimeError('Calibrate the joystick first')

            if self.state['model'] is not None:
                raise RuntimeError('Model is already trained')

            session = self.state['session']

            if (
                session is None
                or session['split'] != 'train'
                or not session['completed']
            ):
                raise RuntimeError(
                    'Complete the 13-sample training round first'
                )

            training_trials = [
                dict(trial)
                for trial in self.trials
                if trial.get('accepted', False)
            ]

            if len(training_trials) != len(BEHAVIORS):
                raise RuntimeError(
                    f'Expected {len(BEHAVIORS)} accepted training trials; '
                    f'found {len(training_trials)}'
                )

            session_id = self.session_id
            calibration = dict(self.calibration)

            self.state['busy'] = True
            self.state['message'] = 'Training model...'

        session_dir = (
            self.data_dir
            / 'data'
            / 'train'
            / session_id
        )

        for trial in training_trials:
            trial['_csv_path'] = (
                session_dir / trial['csv']
            )

        try:
            bundle = train_models(
                training_trials,
                calibration,
            )

            bundle['calibration'] = calibration

            model_path = self.data_dir / 'model.joblib'

            joblib_data = bundle

            joblib.dump(
                joblib_data,
                model_path,
            )

            with self.lock:
                self.model_bundle = bundle

                self.state['model'] = {
                    'trials': len(training_trials),
                    'artifact': str(model_path),
                }

                self.state['busy'] = False
                self.state['message'] = (
                    'Model trained successfully. '
                    'Collect the evaluation round.'
                )

        except Exception:
            with self.lock:
                self.state['busy'] = False
            raise


    def evaluate(self, payload):
        with self.lock:
            if self.model_bundle is None:
                raise RuntimeError(
                    'Train the model before evaluation'
                )

            session = self.state['session']

            if (
                session is None
                or session['split'] != 'validation'
                or not session['completed']
            ):
                raise RuntimeError(
                    'Complete the 13-sample evaluation round first'
                )

            evaluation_trials = [
                dict(trial)
                for trial in self.trials
                if trial.get('accepted', False)
            ]

            if len(evaluation_trials) != len(BEHAVIORS):
                raise RuntimeError(
                    f'Expected {len(BEHAVIORS)} accepted evaluation trials; '
                    f'found {len(evaluation_trials)}'
                )

            session_id = self.session_id
            bundle = self.model_bundle

            self.state['busy'] = True
            self.state['message'] = 'Evaluating frozen model...'

        session_dir = (
            self.data_dir
            / 'data'
            / 'validation'
            / session_id
        )

        for trial in evaluation_trials:
            trial['_csv_path'] = (
                session_dir / trial['csv']
            )

        try:
            results = evaluate_models(
                evaluation_trials,
                bundle,
            )

            results['session_id'] = session_id
            results['evaluated_trials'] = len(
                evaluation_trials
            )
            results['training_artifact'] = str(
                self.data_dir / 'model.joblib'
            )

            plot_paths = save_evaluation_plots(
                results,
                self.data_dir,
            )
            results.update(plot_paths)

            results_path = (
                self.data_dir
                / 'evaluation.json'
            )

            results_path.write_text(
                json.dumps(
                    results,
                    indent=2,
                ),
                encoding='utf-8',
            )

            with self.lock:
                self.state['sealed'] = True
                self.state['busy'] = False
                self.state['message'] = (
                    'Final evaluation complete'
                )

        except Exception:
            with self.lock:
                self.state['busy'] = False
            raise

