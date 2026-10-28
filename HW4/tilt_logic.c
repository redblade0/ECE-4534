#include "tilt_logic.h"
#include <math.h>

#define RAD_TO_TENTHS 572.957795f
#define LEVEL_LIMIT_TENTHS 80
#define DIRECTION_LIMIT_TENTHS 200
#define DOMINANCE_MARGIN_TENTHS 50
#define STABLE_SAMPLES 8U
#define STILL_X_DELTA_COUNTS 35U
#define STILL_Y_DELTA_COUNTS 40U
#define STILL_Z_DELTA_COUNTS 55U
#define ACCEPT_FLASH_SAMPLES 20U
#define REJECT_FLASH_SAMPLES 50U

static int16_t abs16(int16_t value)
{
    return value < 0 ? -value : value;
}

static uint16_t absDiffU16(uint16_t a, uint16_t b)
{
    return (a >= b) ? (uint16_t)(a - b)
                    : (uint16_t)(b - a);
}

static Pose classify(int16_t roll, int16_t pitch)
{
    int16_t ar = abs16(roll);
    int16_t ap = abs16(pitch);
    if (ar <= LEVEL_LIMIT_TENTHS && ap <= LEVEL_LIMIT_TENTHS) {
        return POSE_LEVEL;
    }
    if (ar >= DIRECTION_LIMIT_TENTHS &&
        ar > ap + DOMINANCE_MARGIN_TENTHS) {
        /* Right edge down increases X on this BoosterPack orientation. */
        return roll > 0 ? POSE_RIGHT : POSE_LEFT;
    }
    if (ap >= DIRECTION_LIMIT_TENTHS &&
        ap > ar + DOMINANCE_MARGIN_TENTHS) {
        return pitch < 0 ? POSE_FORWARD : POSE_BACK;
    }
    return POSE_UNKNOWN;
}

static void processStablePose(TiltState *state)
{
    static const Pose expected[5] = {
        POSE_LEFT, POSE_LEVEL, POSE_FORWARD, POSE_LEVEL, POSE_RIGHT
    };
    if (state->step >= 5U || state->stable == POSE_UNKNOWN) return;
    if (state->stable == expected[state->step]) {
        ++state->step;
        state->event = state->step == 5U ? EVENT_UNLOCK : EVENT_ACCEPT;
        state->rejectTicks = 0U;
        state->acceptTicks = ACCEPT_FLASH_SAMPLES;
        return;
    }
    /* At the beginning, level is a neutral rest state. */
    if (state->step == 0U && state->stable == POSE_LEVEL) return;
    state->step = 0U;
    state->event = EVENT_REJECT;
    state->acceptTicks = 0U;
    state->rejectTicks = REJECT_FLASH_SAMPLES;
}

void tiltInit(TiltState *state, int16_t zeroX, int16_t zeroY, int16_t levelZ)
{
    state->zeroX = zeroX;
    state->zeroY = zeroY;
    state->levelZ = levelZ;
    state->candidate = POSE_UNKNOWN;
    state->pending = POSE_UNKNOWN;
    state->stable = POSE_UNKNOWN;

    state->poseStartX = 0U;
    state->poseStartY = 0U;
    state->poseStartZ = 0U;

    state->pendingCount = 0U;
    state->step = 0U;
    state->acceptTicks = 0U;
    state->rejectTicks = 0U;
    state->rollTenths = 0;
    state->pitchTenths = 0;
    state->event = EVENT_NONE;
    state->led = LED_OFF;
}

void tiltUpdate(TiltState *state, uint16_t x, uint16_t y, uint16_t z)
{
    float dx = (float)((int32_t)x - state->zeroX);
    float dy = (float)((int32_t)y - state->zeroY);
    float dz = (float)((int32_t)z - state->levelZ + 512);

    state->rollTenths =
        (int16_t)(atan2f(dx, dz) * RAD_TO_TENTHS);
    state->pitchTenths =
        (int16_t)(atan2f(dy, dz) * RAD_TO_TENTHS);

    state->candidate =
        classify(state->rollTenths, state->pitchTenths);

    state->event = EVENT_NONE;

    if (state->acceptTicks > 0U) {
        --state->acceptTicks;
    }

    if (state->rejectTicks > 0U) {
        --state->rejectTicks;
    }

    if (state->candidate == POSE_UNKNOWN) {
        state->pending = POSE_UNKNOWN;
        state->pendingCount = 0U;
    }
    else {
        if (state->candidate != state->pending) {
            state->pending = state->candidate;

            if (state->pending == state->stable) {
                state->pendingCount = 0U;
            }
            else {
                state->pendingCount = 1U;

                state->poseStartX = x;
                state->poseStartY = y;
                state->poseStartZ = z;
            }
        }
        else if (state->pending != state->stable) {
            if (absDiffU16(x, state->poseStartX) >
                    STILL_X_DELTA_COUNTS ||
                absDiffU16(y, state->poseStartY) >
                    STILL_Y_DELTA_COUNTS ||
                absDiffU16(z, state->poseStartZ) >
                    STILL_Z_DELTA_COUNTS) {

                state->poseStartX = x;
                state->poseStartY = y;
                state->poseStartZ = z;

                state->pendingCount = 1U;
            }
            else if (state->pendingCount < STABLE_SAMPLES) {
                ++state->pendingCount;
            }

            if (state->pendingCount >= STABLE_SAMPLES) {
                state->stable = state->pending;
                state->pendingCount = 0U;

                processStablePose(state);
            }
        }
    }

    if (state->step == 5U) {
        state->led = LED_GREEN;
    }
    else if (state->rejectTicks > 0U) {
        state->led = LED_RED;
    }
    else if (state->acceptTicks > 0U) {
        state->led = LED_GREEN;
    }
    else if (state->stable == POSE_LEVEL) {
        state->led = LED_BLUE;
    }
    else if (state->stable != POSE_UNKNOWN) {
        state->led = LED_RED;
    }
    else {
        state->led = LED_OFF;
    }
}
