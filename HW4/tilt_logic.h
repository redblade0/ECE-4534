#ifndef TILT_LOGIC_H
#define TILT_LOGIC_H

#include <stdint.h>

typedef enum {
    POSE_UNKNOWN = '?', POSE_LEVEL = '0', POSE_LEFT = 'L',
    POSE_RIGHT = 'R', POSE_FORWARD = 'F', POSE_BACK = 'B'
} Pose;

typedef enum {
    LED_OFF = '-', LED_RED = 'R', LED_GREEN = 'G', LED_BLUE = 'B'
} LedCommand;

typedef enum {
    EVENT_NONE = '-', EVENT_ACCEPT = 'A', EVENT_REJECT = 'X',
    EVENT_UNLOCK = 'U'
} TiltEvent;

typedef struct {
    int16_t zeroX;
    int16_t zeroY;
    int16_t levelZ;
    Pose candidate;
    Pose pending;
    Pose stable;
    uint8_t pendingCount;
    uint16_t poseStartX;
    uint16_t poseStartY;
    uint16_t poseStartZ;
    uint8_t step;
    uint8_t acceptTicks;
    uint8_t rejectTicks;
    int16_t rollTenths;
    int16_t pitchTenths;
    TiltEvent event;
    LedCommand led;
} TiltState;

void tiltInit(TiltState *state, int16_t zeroX, int16_t zeroY, int16_t levelZ);
void tiltUpdate(TiltState *state, uint16_t x, uint16_t y, uint16_t z);

#endif
