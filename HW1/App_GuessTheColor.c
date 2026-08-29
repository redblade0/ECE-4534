// /*
//  * Copyright (c) 2021, Texas Instruments Incorporated
//  * All rights reserved.
//  *
//  * Redistribution and use in source and binary forms, with or without
//  * modification, are permitted provided that the following conditions
//  * are met:
//  *
//  * *  Redistributions of source code must retain the above copyright
//  *    notice, this list of conditions and the following disclaimer.
//  *
//  * *  Redistributions in binary form must reproduce the above copyright
//  *    notice, this list of conditions and the following disclaimer in the
//  *    documentation and/or other materials provided with the distribution.
//  *
//  * *  Neither the name of Texas Instruments Incorporated nor the names of
//  *    its contributors may be used to endorse or promote products derived
//  *    from this software without specific prior written permission.
//  *
//  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
//  * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
//  * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
//  * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
//  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//  */
#include "ti_msp_dl_config.h"

/* Standard Includes */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* HAL and Application includes */
#include <HAL/HAL.h>
#include <HAL/Timer.h>
#include <HAL/Display.h>
#include <App_GuessTheColor.h>

/**
 * The main entry point of your project. The main function should immediately
 * stop the Watchdog timer, call the Application constructor, and then
 * repeatedly call the main super-loop function. The App_GuessTheColor
 * constructor should be responsible for initializing all hardware components as
 * well as all other finite state machines you choose to use in this project.
 *
 * THIS FUNCTION IS ALREADY COMPLETE. Unless you want to temporarily experiment
 * with some behavior of a code snippet you may have, we DO NOT RECOMMEND
 * modifying this function in any way.
 */
int main(void)
{
    SYSCFG_DL_init();
    Graphics_Init();
    InitSystemTiming();

    HAL hal = HAL_construct();

    App_GuessTheColor app = App_GuessTheColor_construct(&hal);

    App_GuessTheColor_showTitleScreen();

    while (true)
    {
        App_GuessTheColor_loop(&app, &hal);
        HAL_refresh(&hal);
    }
}

/**
 * The main constructor for your application. This function initializes each
 * state variable required for the GuessTheColor game.
 */
App_GuessTheColor App_GuessTheColor_construct(HAL* hal_p)
{
    // The App_GuessTheColor object to initialize
    App_GuessTheColor app;

    // Predetermined random numbers for this application. In an actual project,
    // you should probably use some form of noise generator instead, like the
    // noise from your ADC.
    app.randomNumbers[0] = 5;
    app.randomNumbers[1] = 2;
    app.randomNumbers[2] = 7;
    app.randomNumbers[3] = 1;
    app.randomNumbers[4] = 3;

    app.randomNumberChoice = 0;

    // Initialization of FSM variables
    app.state = TITLE_SCREEN;
    app.timer = SWTimer_construct(TITLE_SCREEN_WAIT);
    SWTimer_start(&app.timer);

    App_GuessTheColor_initGameVariables(&app, hal_p);

    app.cursor = CURSOR_0;

    // Return the completed Application struct to the user
    return app;
}

/**
 * The main super-loop function of the application. We place this inside of a
 * single infinite loop in main. In this way, we can model a polling system of
 * FSMs. Every cycle of this loop function, we poll each of the FSMs one time.
 */
void App_GuessTheColor_loop(App_GuessTheColor* app_p, HAL* hal_p)
{
    switch (app_p->state)
    {
        case TITLE_SCREEN:
            App_GuessTheColor_handleTitleScreen(app_p, hal_p);
            break;

        case INSTRUCTIONS_SCREEN:
            App_GuessTheColor_handleInstructionsScreen(app_p, hal_p);
            break;

        case GAME_SCREEN:
            App_GuessTheColor_handleGameScreen(app_p, hal_p);
            break;

        case RESULT_SCREEN:
            App_GuessTheColor_handleResultScreen(app_p, hal_p);
            break;

        default:
            break;
    }
}

/**
 * Sets up the GuessTheColors game by initializing the game state to the Title
 * Screen state.
 */
void App_GuessTheColor_showTitleScreen()
{

    Graphics_FillScreen(GRAPHICS_BLACK);
    // Graphics_drawImage(&gfx_p->context, &colors8BPP_UNCOMP, 0, 0);

    Graphics_DrawString(5, 10, "Guess the RGB color", GRAPHICS_WHITE);
    Graphics_DrawString(5, 25, "-------------------", GRAPHICS_WHITE);
    Graphics_DrawString(5, 40, "By: Leyla Nazhand-Ali", GRAPHICS_WHITE);
    Graphics_DrawString(5, 55, "Edit: Matthew Zhong", GRAPHICS_WHITE);
}

/**
 * A helper function which resets all the game variables to their unselected
 * states and resets the cursor position.
 */
void App_GuessTheColor_initGameVariables(App_GuessTheColor* app_p, HAL* hal_p)
{
    // Reset the cursor
    app_p->cursor = CURSOR_0;

    // Deselect each option
    app_p->redSelected = false;
    app_p->greenSelected = false;
    app_p->blueSelected = false;

    // Turn off all LEDs - they don't turn on until a random number is generated
    LED_turnOff(&hal_p->boosterpackRed);
    LED_turnOff(&hal_p->boosterpackGreen);
    LED_turnOff(&hal_p->boosterpackBlue);
}

/**
 * Callback function for when the game is in the TITLE_SCREEN state. Used to
 * break down the main App_GuessTheColor_loop() function into smaller
 * sub-functions.
 */
void App_GuessTheColor_handleTitleScreen(App_GuessTheColor* app_p, HAL* hal_p)
{
    if (SWTimer_expired(&app_p->timer))
    {
        app_p->state = INSTRUCTIONS_SCREEN;
        App_GuessTheColor_showInstructionsScreen(app_p);

    }
}

/**
 * Callback function for when the game is in the INSTRUCTIONS_SCREEN state. Used
 * to break down the main App_GuessTheColor_loop() function into smaller
 * sub-functions.
 */
void App_GuessTheColor_handleInstructionsScreen(App_GuessTheColor* app_p, HAL* hal_p)
{
    // Transition to start the game when B2 is pressed
    if (Button_isTapped(&hal_p->boosterpackS2))
    {
        // Update internal logical state
        app_p->state = GAME_SCREEN;

        // Turn on LEDs based off of the lowest three bits of a random number.
        uint32_t randomNumber = app_p->randomNumbers[app_p->randomNumberChoice];

        if (randomNumber & 1 << 0) { LED_turnOn(&hal_p->boosterpackRed  ); }
        if (randomNumber & 1 << 1) { LED_turnOn(&hal_p->boosterpackGreen); }
        if (randomNumber & 1 << 2) { LED_turnOn(&hal_p->boosterpackBlue ); }

        // Increment the random number choice with a mod loopback to 0 when reaching
        // NUM_RANDOM_NUMBERS.
        app_p->randomNumberChoice = (app_p->randomNumberChoice + 1) % NUM_RANDOM_NUMBERS;

        // Display the next state's screen to the user
        App_GuessTheColor_showGameScreen(app_p);
    }
}

/**
 * Callback function for when the game is in the GAME_SCREEN state. Used to
 * break down the main App_GuessTheColors_loop() function into smaller
 * sub-functions.
 */
void App_GuessTheColor_handleGameScreen(App_GuessTheColor* app_p, HAL* hal_p)
{
    // If B2 is pressed, increment the cursor and circle it around to 0 if it
    // reaches the bottom
    if (Button_isTapped(&hal_p->boosterpackS2)) {
        app_p->cursor = (Cursor) (((int) app_p->cursor + 1) % NUM_TEST_OPTIONS);
        App_GuessTheColor_updateGameScreen(app_p);
    }

    // If B1 is pressed, either add a selection to the proper color choice OR
    // transition to the SHOW_RESULT state if the user chooses to end the test.
    if (Button_isTapped(&hal_p->boosterpackS1))
    {
        switch (app_p->cursor)
        {
            // In the first three choices, we need to re-display the game screen
            // to reflect updated choices.
            // -----------------------------------------------------------------
            case CURSOR_0: // Red choice
                app_p->redSelected = true;
                App_GuessTheColor_updateGameScreen(app_p);
                break;

            case CURSOR_1: // Green choice
                app_p->greenSelected = true;
                App_GuessTheColor_updateGameScreen(app_p);
                break;

            case CURSOR_2: // Blue choice
                app_p->blueSelected = true;
                App_GuessTheColor_updateGameScreen(app_p);
                break;

            // In the final choice, we must setup a transition to RESULT_SCREEN
            // by starting a timer and calling the proper draw function.
            // -----------------------------------------------------------------
            case CURSOR_3:
                app_p->state = RESULT_SCREEN;

                app_p->timer = SWTimer_construct(RESULT_SCREEN_WAIT);
                SWTimer_start(&app_p->timer);

                App_GuessTheColor_showResultScreen(app_p, hal_p);
                break;

            default:
                break;
        }
    }
}

/**
 * Callback function for when the game is in the RESULT_SCREEN state. Used to
 * break down the main App_GuessTheColor_loop() function into smaller
 * sub-functions.
 */
void App_GuessTheColor_handleResultScreen(App_GuessTheColor* app_p, HAL* hal_p)
{
    // Transition to instructions and reset game variables when the timer expires
    if (SWTimer_expired(&app_p->timer))
    {
        app_p->state = INSTRUCTIONS_SCREEN;
        App_GuessTheColor_initGameVariables(app_p, hal_p);
        App_GuessTheColor_showInstructionsScreen(app_p);
    }
}

/**
 * A helper function which clears the screen and prints the instructions for how
 * to play the game.
 */
void App_GuessTheColor_showInstructionsScreen(App_GuessTheColor* app_p)
{
    // Clear the screen from any old text state
    Graphics_FillScreen(GRAPHICS_BLACK);

    // Display the text
    Graphics_DrawString(5, 10, "Instructions", GRAPHICS_WHITE);
    Graphics_DrawString(5, 20, "-------------------", GRAPHICS_WHITE);
    Graphics_DrawString(5, 35, "Guess the RGB mix.", GRAPHICS_WHITE);
    Graphics_DrawString(5, 50, "Controls:", GRAPHICS_WHITE);
    Graphics_DrawString(5, 65, "B1: Select choice", GRAPHICS_WHITE);
    Graphics_DrawString(5, 80, "B2: Move arrow", GRAPHICS_WHITE);
    Graphics_DrawString(5, 100, "Press B2 to start.", GRAPHICS_WHITE);
}

/**
 * A helper function which clears the screen and draws an updated display of
 * each color and its selection, intended for use when setting up the
 * GAME_SCREEN state.
 */
void App_GuessTheColor_showGameScreen(App_GuessTheColor* app_p)
{
    Graphics_FillScreen(GRAPHICS_BLACK);

    Graphics_DrawString(5, 10, "Game", GRAPHICS_WHITE);
    Graphics_DrawString(5, 20, "-------------------", GRAPHICS_WHITE);

    Graphics_DrawString(5, 35, "  Red", GRAPHICS_WHITE);
    Graphics_DrawString(5, 50, "  Green", GRAPHICS_WHITE);
    Graphics_DrawString(5, 65, "  Blue", GRAPHICS_WHITE);
    Graphics_DrawString(5, 80, "  End Guessing", GRAPHICS_WHITE);

    Graphics_DrawString(5, 100, "B1: Select", GRAPHICS_WHITE);
    Graphics_DrawString(5, 115, "B2: Move", GRAPHICS_WHITE);

    // Draw the ursor
    Graphics_DrawString(5, 35 + (app_p->cursor * 15), ">", GRAPHICS_WHITE);

    // Draw the stars for LED guesses
    if (app_p->redSelected  ) { Graphics_DrawString(45, 35, "  *", GRAPHICS_WHITE); }
    if (app_p->greenSelected) { Graphics_DrawString(45, 50, "  *", GRAPHICS_WHITE); }
    if (app_p->blueSelected ) { Graphics_DrawString(45, 65, "  *", GRAPHICS_WHITE); }
}

/**
 * A helper function which updates the main game screen by redrawing only the
 * positions where the cursor could possibly be updated.
 */
void App_GuessTheColor_updateGameScreen(App_GuessTheColor* app_p)
{
    // Clear the cursors from any previous game screen
    Graphics_DrawString(5, 35, ">", GRAPHICS_BLACK);
    Graphics_DrawString(5, 50, ">", GRAPHICS_BLACK);
    Graphics_DrawString(5, 65, ">", GRAPHICS_BLACK);
    Graphics_DrawString(5, 80, ">", GRAPHICS_BLACK);

    // Draw the cursor
    Graphics_DrawString(5, 35 + (app_p->cursor * 15), ">", GRAPHICS_WHITE);

    // Draw the stars for LED guesses
    if (app_p->redSelected  ) { Graphics_DrawString(45, 35, "  *", GRAPHICS_WHITE); }
    if (app_p->greenSelected) { Graphics_DrawString(45, 50, "  *", GRAPHICS_WHITE); }
    if (app_p->blueSelected ) { Graphics_DrawString(45, 65, "  *", GRAPHICS_WHITE); }
}

/**
 * A helper function which clears the screen and displays whether the user has
 * won or not.
 */
void App_GuessTheColor_showResultScreen(App_GuessTheColor* app_p, HAL* hal_p)
{
    // Print the splash text
    Graphics_FillScreen(GRAPHICS_BLACK);
    Graphics_DrawString(5, 10, "Result", GRAPHICS_WHITE);
    Graphics_DrawString(5, 20, "-------------------", GRAPHICS_WHITE);

    // Determine if each selection matched correctly
    bool match = app_p->redSelected   == LED_isLit(&hal_p->boosterpackRed  )
              && app_p->greenSelected == LED_isLit(&hal_p->boosterpackGreen)
              && app_p->blueSelected  == LED_isLit(&hal_p->boosterpackBlue );

    // Print the correct string based on if the user won or not
    if (match) {
        Graphics_DrawString(5, 40, "You Win!", GRAPHICS_WHITE);
    }
    else {
        Graphics_DrawString(5, 40, "You Lose", GRAPHICS_WHITE);
    }
}