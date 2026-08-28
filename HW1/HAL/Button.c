#include <HAL/Button.h>

/*
    GPIO Configuration is handled by sysconfig.
    S1 maps to PA11, pull-up resistor
    S2 maps to PA12, pull-up resistor
*/
Button Button_construct(GPIO_Regs *port, uint32_t pin)
{
    Button button;

    button.port = port;
    button.pin = pin;

    button.debounceState = StableR;

    button.timer = SWTimer_construct(DEBOUNCE_TIME_MS);
    SWTimer_start(&button.timer);

    button.pushState = RELEASED;
    button.isTapped = false;

    return button;
}

/**
    A getter method to return if the user holds down the button.

    @param button_p:    The Button object from which to retrieve the push state

    @return true if the button is depressed, and false if it is not
 */
bool Button_isPressed(Button* button_p)
{
    return button_p->pushState == PRESSED;
}

/**
    A getter method to return if the user has tapped the button.

    @param button:   The Button object from which to retrieve the tapped state

    @return true if the button was tapped, and false otherwise
 */
bool Button_isTapped(Button* button_p)
{
    return button_p->isTapped;
}


/**
 * Refreshes the input of the provided Button by polling for the new GPIO input
 * pin value and advancing the debouncing FSM by one step.
 *
 * @param button_p:   The Button object to refresh
 */
void Button_refresh(Button* button_p)
{

    uint32_t rawButtonStatus = DL_GPIO_readPins(button_p->port, button_p->pin);

    bool pressed = (rawButtonStatus == 0);

    int newPushState = RELEASED;

    switch (button_p->debounceState)
    {
        // Released State - transition only if the new raw state is pressed
        case StableR:
            if (pressed)
            {
                SWTimer_start(&button_p->timer);
                button_p->debounceState = TransitionRP;
            }

            newPushState = RELEASED;
            break;

        // Pressed State - transition only if the new raw state is released
        case StableP:
            if (!pressed)
            {
                SWTimer_start(&button_p->timer);
                button_p->debounceState = TransitionPR;
            }

            newPushState = PRESSED;
            break;

        // Transition State - transition if either the timer expires OR if
        //                    the input becomes polluted with an erroneous
        //                    RELEASED input.
        case TransitionRP:
            if (!pressed)
            {
                button_p->debounceState = StableR;
            }
            else if (SWTimer_expired(&button_p->timer))
            {
                button_p->debounceState = StableP;
            }
            newPushState = RELEASED;
            break;

        // Transition State - transition if either the timer expires OR if
        //                    the input becomes polluted with an erroneous
        //   
        case TransitionPR:
            if (pressed)
            {
                button_p->debounceState = StableP;
            }
            else if (SWTimer_expired(&button_p->timer))
            {
                button_p->debounceState = StableR;
            }

            newPushState = PRESSED;
            break;
    }

    // Outputs of the FSM: The button is tapped if the old debounced state was
    // RELEASED and the new state is PRESSED.
    button_p->isTapped = (newPushState == PRESSED && button_p->pushState == RELEASED);
    button_p->pushState = newPushState;
}
