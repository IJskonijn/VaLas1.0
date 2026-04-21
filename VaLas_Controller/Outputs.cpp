#include <Arduino.h>
#include "Outputs.h"
#include "VaLas_Controller.h"

int elrToggleState = 0;
int old_elrToggleState = 0;
bool elrEnabled = false;

Outputs::Outputs()
{
}

/// Optional stuff for now

void Outputs::ToggleElrHighIdle()
{
  // Set pwm signal to mechanical pump ELR pins

  elrToggleState = digitalRead(elrTogglePin);
  if ((elrToggleState == 0) && (old_elrToggleState == 1))
  {
    Serial.println("Toggle high idle via PWM");
    elrEnabled = !elrEnabled;
    int pwmVal = elrEnabled ? 125 : 0;
    ledcWrite(elrChannel, pwmVal);
  }
  old_elrToggleState = elrToggleState;
}

//void Outputs::HornPressed()
//{
//    // Works together with paddle shifters, so we can use 1 wire for both horn and paddle shifter input.
//    bool hornPressed = analogRead(hornPin) == 0;
//    if (hornPressed)    {
//        Serial.println("Horn pressed");
//        // do some horn stuff
//        // switch relay on while horn is pressed, switch off when released
//    }
//}

// void sendSpeedSignalToSpeedometer()
// {
//     // do some sensor reading and outputting to W126 speedo
// }
