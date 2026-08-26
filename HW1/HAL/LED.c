#include <HAL/LED.h>

LED LED_construct(GPIO_Regs *port, uint32_t pin)
{
    LED led;

    led.port = port;
    led.pin = pin;
    led.isLit = false;

    DL_GPIO_clearPins(led.port, led.pin);

    return led;
}

void LED_turnOn(LED* led_p)
{
    led_p->isLit = true;

    DL_GPIO_setPins(led_p->port, led_p->pin);
}

void LED_turnOff(LED* led_p)
{
    led_p->isLit = false;

    DL_GPIO_clearPins(led_p->port, led_p->pin);
}

void LED_toggle(LED* led_p)
{
    led_p->isLit = !led_p->isLit;

    DL_GPIO_togglePins(led_p->port, led_p->pin);
}

bool LED_isLit(LED* led_p)
{
    return led_p->isLit;
}