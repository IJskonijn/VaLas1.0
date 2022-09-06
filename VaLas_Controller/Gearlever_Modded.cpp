
// GEARLEVER SETTINGS
// 3.3V Pin on ESP32 + sensor pin 4
// Reads ranges:
// P   300-600
// R   1600-2000
// N   2100-2500
// D   3000-3400

#include <Arduino.h>
#include "Gearlever_Modded.h"
#include "TaskStructs.h"

int up_shift = 0;
int down_shift = 0;
int old_upshift = 0;
int old_downshift = 0;

VaLas_Controller::ShiftRequest* currentShiftRequestValue;

Gearlever_Modded::Gearlever_Modded()
{
  Serial.println("Using modded gearlever");
}

void Gearlever_Modded::ReadGearLever(void * parameter)
{
  TaskStructs::GearLeverParameters *parameters = (TaskStructs::GearLeverParameters*) parameter;   
  VaLas_Controller::GearLeverPosition* currentLeverPosition = parameters->currentLeverPositionPtr;
  VaLas_Controller::GearLeverPosition* oldLeverPosition = parameters->oldLeverPositionPtr;
  currentShiftRequestValue = parameters->currentShiftRequestPtr;

  *oldLeverPosition = *currentLeverPosition;
  readGearLeverPosition(currentLeverPosition);
  readShiftRequest(currentLeverPosition);
}

void Gearlever_Modded::Reset()
{
  // Reset all shifting vars
  up_shift = 1;
  down_shift = 1;
  old_upshift = 0;
  old_downshift = 0;
}

void Gearlever_Modded::CompleteShiftRequest()
{
  *currentShiftRequestValue = VaLas_Controller::ShiftRequest::NoShift;
}

void Gearlever_Modded::readGearLeverPosition(VaLas_Controller::GearLeverPosition* currentLeverPosition)
{
  int leverValue = analogRead(gearLeverPotPin);
  vTaskDelay(15);
  Serial.println("lever sensor val: " + String(leverValue));

  switch (leverValue)
  {
  case 200 ... 600:
    *currentLeverPosition = VaLas_Controller::GearLeverPosition::Park;
    break;
  case 1400 ... 2000:
    *currentLeverPosition = VaLas_Controller::GearLeverPosition::Reverse;
    break;
  case 2100 ... 2500:
    *currentLeverPosition = VaLas_Controller::GearLeverPosition::Neutral;
    break;
  case 2900 ... 3400:
    *currentLeverPosition = VaLas_Controller::GearLeverPosition::Drive;
    break;
  
  default: // I guess something went wrong...
    break;
  }

  Serial.println("Leverposition updated to " + String((int)*currentLeverPosition));
  //vTaskDelay(50); // delay(50);
}

void Gearlever_Modded::readShiftRequest(VaLas_Controller::GearLeverPosition* currentLeverPosition)
{
  Serial.println("readShiftRequest > currentleverpos: " + String((int) *currentLeverPosition ));
  Serial.println("readShiftRequest > currentshiftreq: " + String((int) *currentShiftRequestValue ));

  // Wait for ShiftControl to set it back to NoShift.
  // Only then continue with setting a new shiftrequest
  if (*currentShiftRequestValue != VaLas_Controller::ShiftRequest::NoShift)
    return;

  // Do nothing if not in Drive
  if (*currentLeverPosition != VaLas_Controller::GearLeverPosition::Drive)
    return;

  // check ShiftRequest::UpShift transition
  up_shift = digitalRead(upShiftPin);
  if ((up_shift == 0) && (old_upshift == 1))
  {
    *currentShiftRequestValue = VaLas_Controller::ShiftRequest::UpShift;
    //vTaskDelay(50); // delay(50);
    Serial.println("Upshift pressed");
  }
  old_upshift = up_shift;

  // check ShiftRequest::DownShift transition
  down_shift = digitalRead(downShiftPin);
  if ((down_shift == 0) && (old_downshift == 1))
  {
    *currentShiftRequestValue = VaLas_Controller::ShiftRequest::DownShift;
    //vTaskDelay(50); // delay(50);
    Serial.println("Downshift pressed");
  }
  old_downshift = down_shift;
}
