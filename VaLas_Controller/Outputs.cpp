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

void toggleElrHighIdle()
{
  // Set pwm signal to mechanical pump ELR pins

  elrToggleState = digitalRead(upShiftPin);
  if ((elrToggleState == 0) && (old_elrToggleState == 1))
  {
    Serial.println("Toggle high idle via PWM");
    elrEnabled = !elrEnabled;
    int pwmVal = elrEnabled ? 125 : 0;
    ledcWrite(elrChannel, pwmVal);
  }
  old_elrToggleState = elrToggleState;
}

// void sendSpeedSignalToSpeedometer()
// {
//     // do some sensor reading and outputting to W126 speedo
// }