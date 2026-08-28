#ifndef HAL_BUTTON_H_
#define HAL_BUTTON_H_

#include <HAL/Timer.h>
#include <ti_msp_dl_config.h>
#include <stdbool.h>
#include <stdint.h>

#define DEBOUNCE_TIME_MS    5
#define PRESSED             0
#define RELEASED            1

enum _DebounceState
{
    StableP,
    TransitionPR,
    TransitionRP,
    StableR
};

typedef enum _DebounceState DebounceState;

struct _Button
{
    GPIO_Regs *port;
    uint32_t pin;

    DebounceState debounceState;

    SWTimer timer;

    int pushState;
    bool isTapped;
};

typedef struct _Button Button;

Button Button_construct(GPIO_Regs *port, uint32_t pin);

bool Button_isPressed(Button* button);

bool Button_isTapped(Button* button);

void Button_refresh(Button* button);

#endif /* HAL_BUTTON_H_ */
