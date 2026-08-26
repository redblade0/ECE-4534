#ifndef HAL_LED_H_
#define HAL_LED_H_

#include <stdbool.h>
#include "ti_msp_dl_config.h"

struct _LED
{
    GPIO_Regs *port; // Use GPIO Regs becuase port now is a pointer to a struct instead of an enum
    uint32_t pin;
    bool isLit;
};

typedef struct _LED LED;

LED LED_construct(GPIO_Regs *port, uint32_t pin);

void LED_turnOn(LED* led_p);

void LED_turnOff(LED* led_p);

void LED_toggle(LED* led_p);

bool LED_isLit(LED* led_p);

#endif /* HAL_LED_H_ */