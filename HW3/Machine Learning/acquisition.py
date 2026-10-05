import threading
import time
import serial


def parse_packet(line):
    if isinstance(line, bytes):
        line = line.decode('ascii', errors='strict')

    line = line.strip()

    parts = line.split(',')
    if len(parts) != 3:
        raise ValueError('Expected sample_number,x_raw,y_raw')

    try:
        sample_number = int(parts[0])
        x = int(parts[1])
        y = int(parts[2])
    except ValueError:
        raise ValueError('Packet contains non-integer values')

    if not 0 <= sample_number <= 0xFFFFFFFF:
        raise ValueError('Sample number out of range')

    if not 0 <= x <= 4095:
        raise ValueError('X value out of range')

    if not 0 <= y <= 4095:
        raise ValueError('Y value out of range')

    return sample_number, x, y


class SerialReader:
    def __init__(self, port, on_sample, on_status):
        self.port = port
        self.on_sample = on_sample
        self.on_status = on_status
        self.serial = None
        self.thread = None
        self.stop_event = threading.Event()

    def start(self):
        if self.thread and self.thread.is_alive():
            return

        self.stop_event.clear()

        self.thread = threading.Thread(
            target=self._run,
            name='serial-reader',
            daemon=True
        )

        self.thread.start()

    def _run(self):
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=115200,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.25
            )

            self.on_status({
                'connected': True,
                'message': f'Connected to {self.port}'
            })

            last_sample = None
            last_time = None
            last_data_time = time.monotonic()
            stream_reported_stale = False

            good = 0
            bad = 0
            gaps = 0

            while not self.stop_event.is_set():
                try:
                    line = self.serial.readline()
                    now = time.monotonic()

                    if not line:
                        # A serial handle can remain open even when the board
                        # stops sending. Detect that condition explicitly.
                        if (
                            last_sample is not None
                            and not stream_reported_stale
                            and now - last_data_time > 1.0
                        ):
                            stream_reported_stale = True
                            self.on_status({
                                'connected': False,
                                'message': 'No joystick data received'
                            })
                        continue

                    timestamp = now
                    last_data_time = timestamp
                    stream_reported_stale = False

                    try:
                        sample_number, x, y = parse_packet(line)
                    except (ValueError, UnicodeDecodeError):
                        bad += 1

                        self.on_status({
                            'connected': True,
                            'bad': bad
                        })

                        continue

                    gap = False
                    reset = False

                    if last_sample is not None:
                        expected = (last_sample + 1) & 0xFFFFFFFF

                        if sample_number != expected:
                            if (
                                last_sample == 0xFFFFFFFF
                                and sample_number == 0
                            ):
                                pass
                            elif sample_number < last_sample:
                                reset = True
                            else:
                                gap = True
                                gaps += sample_number - last_sample - 1

                    rate = 0.0

                    if last_time is not None:
                        dt = timestamp - last_time

                        if dt > 0:
                            rate = 1.0 / dt

                    good += 1

                    self.on_sample(
                        sample_number,
                        x,
                        y,
                        timestamp,
                        rate,
                        gap,
                        reset
                    )

                    if good % 10 == 0:
                        self.on_status({
                            'connected': True,
                            'good': good,
                            'bad': bad,
                            'gaps': gaps,
                            'rate': rate
                        })

                    last_sample = sample_number
                    last_time = timestamp

                except serial.SerialException as error:
                    self.on_status({
                        'connected': False,
                        'message': f'Serial connection lost: {error}'
                    })

                    break

        except serial.SerialException as error:
            self.on_status({
                'connected': False,
                'message': f'Could not open {self.port}: {error}'
            })

        finally:
            if self.serial is not None:
                try:
                    self.serial.close()
                except Exception:
                    pass

            self.serial = None

            if not self.stop_event.is_set():
                self.on_status({
                    'connected': False,
                    'message': 'Board disconnected'
                })

    def close(self):
        self.stop_event.set()

        if self.serial is not None:
            try:
                self.serial.close()
            except Exception:
                pass

        if self.thread and self.thread.is_alive():
            self.thread.join(timeout=1.0)

        self.serial = None
