"""Supplied dashboard vocabulary and empty state; contains no recognition logic."""
DIRECTIONS = ('center', 'north', 'northeast', 'east', 'southeast', 'south',
              'southwest', 'west', 'northwest')
MOTIONS = ('still', 'horizontal', 'vertical', 'clockwise', 'anticlockwise')
BEHAVIORS = DIRECTIONS + MOTIONS[1:]
SAMPLE_RATE_HZ = 50
CALIBRATION_SAMPLES = 100
TRIAL_SAMPLES = 300
MOTION_WINDOW_SAMPLES = 100
LIVE_HOP_SAMPLES = 10

def empty_state(port):
    return {
        'connected': False, 'port': port, 'raw': None, 'path': [],
        'calibration': {}, 'calibrated': False,
        'result': {'position': 'not trained', 'motion': 'not trained'},
        'job': None, 'session': None, 'model': None,
        'message': 'Student starter: implement board acquisition and backend workflow.',
        'good': 0, 'bad': 0, 'gaps': 0, 'rate': 0.0,
        'busy': False, 'sealed': False,
    }
