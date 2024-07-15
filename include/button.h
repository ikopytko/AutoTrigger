// Based on https://forum.arduino.cc/t/button-function-click-double-click-more/14544
#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

class Button
{
public:
    Button(uint8_t pin) : butttonPin{pin} {};

    short checkButton()
    {
        short event = NONE;
        buttonVal = digitalRead(butttonPin);
        // Button pressed down
        if (buttonVal == LOW && buttonLast == HIGH && (millis() - upTime) > debounce)
        {
            downTime = millis();
            ignoreUp = false;
            waitForUp = false;
            singleOK = true;
            holdEventPast = false;
            longHoldEventPast = false;
            if ((millis() - upTime) < DCgap && DConUp == false && DCwaiting == true)
                DConUp = true;
            else
                DConUp = false;
            DCwaiting = false;
        }
        // Button released
        else if (buttonVal == HIGH && buttonLast == LOW && (millis() - downTime) > debounce)
        {
            if (not ignoreUp)
            {
                upTime = millis();
                if (DConUp == false)
                    DCwaiting = true;
                else
                {
                    event = DOUBLE_CLICK;
                    DConUp = false;
                    DCwaiting = false;
                    singleOK = false;
                }
            }
        }
        // Test for normal click event: DCgap expired
        if (buttonVal == HIGH && (millis() - upTime) >= DCgap && DCwaiting == true && DConUp == false && singleOK == true && event != 2)
        {
            event = CLICK;
            DCwaiting = false;
        }
        // Test for hold
        if (buttonVal == LOW && (millis() - downTime) >= holdTime)
        {
            // Trigger "normal" hold
            if (not holdEventPast)
            {
                event = HOLD;
                waitForUp = true;
                ignoreUp = true;
                DConUp = false;
                DCwaiting = false;
                // downTime = millis();
                holdEventPast = true;
            }
            // Trigger "long" hold
            if ((millis() - downTime) >= longHoldTime)
            {
                if (not longHoldEventPast)
                {
                    event = LONG_HOLD;
                    longHoldEventPast = true;
                }
            }
        }
        buttonLast = buttonVal;
        return event;
    }

    static const short NONE = 0;
    static const short CLICK = 1;
    static const short DOUBLE_CLICK = 2;
    static const short HOLD = 3;
    static const short LONG_HOLD = 4;

private:
    uint8_t butttonPin;

    // Button timing variables
    uint16_t debounce = 20;       // ms debounce period to prevent flickering when pressing or releasing the button
    uint16_t DCgap = 250;         // max ms between clicks for a double click event
    uint16_t holdTime = 1000;     // ms hold period: how long to wait for press+hold event
    uint16_t longHoldTime = 3000; // ms long hold period: how long to wait for press+hold event

    // Button variables
    boolean buttonVal = HIGH;          // value read from button
    boolean buttonLast = HIGH;         // buffered value of the button's previous state
    boolean DCwaiting = false;         // whether we're waiting for a double click (down)
    boolean DConUp = false;            // whether to register a double click on next release, or whether to wait and click
    boolean singleOK = true;           // whether it's OK to do a single click
    uint64_t downTime = -1;            // time the button was pressed down
    uint64_t upTime = -1;              // time the button was released
    boolean ignoreUp = false;          // whether to ignore the button release because the click+hold was triggered
    boolean waitForUp = false;         // when held, whether to wait for the up event
    boolean holdEventPast = false;     // whether or not the hold event happened already
    boolean longHoldEventPast = false; // whether or not the long hold event happened already
};

#endif