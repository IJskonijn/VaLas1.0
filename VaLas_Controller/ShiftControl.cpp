#include <Arduino.h>
#include "TaskStructs.h"
#include "ShiftControl.h"
#include "ShiftConfig.h"
#include "Sensors.h"
#include "Gearlever.h"
#include "Gearlever_CAN.h"
#include "Gearlever_Modded.h"

VaLas_Controller::PwmChannels* pwmChannelsPointer;
DisplayHandler* displayHandlerPointer;
Gearlever* gearlever;

VaLas_Controller::DisplayScreen* screenToDisplayValue;
VaLas_Controller::ShiftSetting* gearboxSettings;


void ShiftControl::init(DisplayHandler* displayHandlerPtr, VaLas_Controller::PwmChannels* pwmChannelsPtr, Gearlever* gearLeverPtr, VaLas_Controller::DisplayScreen* screenToDisplayPtr, VaLas_Controller::ShiftSetting* gearboxSettingsPtr)
{
  Serial.println("Init ShiftControl");
  displayHandlerPointer = displayHandlerPtr;
  pwmChannelsPointer = pwmChannelsPtr;
  gearlever = gearLeverPtr;
  screenToDisplayValue = screenToDisplayPtr;
  gearboxSettings = gearboxSettingsPtr;
}

void ShiftControl::execute(void * parameter)
{ 
  TaskStructs::ShiftControlParameters *parameters = (TaskStructs::ShiftControlParameters*) parameter;
  int gear = *(parameters->gearPtr);
  //VaLas_Controller::ShiftSetting* gearboxSettings = parameters->shiftSettings;
  VaLas_Controller::GearLeverPosition oldLeverPosition = *(parameters->oldLeverPositionPtr);
  VaLas_Controller::GearLeverPosition currentLeverPosition = *(parameters->currentLeverPositionPtr);
  VaLas_Controller::ShiftRequest currentShiftRequest = *(parameters->currentShiftRequestPtr);

  processLeverValues(oldLeverPosition, currentLeverPosition, gear);

  if (currentLeverPosition != VaLas_Controller::GearLeverPosition::Drive || currentShiftRequest == VaLas_Controller::ShiftRequest::NoShift)
  {
    Serial.println("No shiftrequest");
    return; // Nothing to do if there is no shiftrequest 
  }

  // Check for the up_shift
  if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Drive && currentShiftRequest == VaLas_Controller::ShiftRequest::UpShift)
  {
    Serial.println("Upshift detected");
    Serial.println("Current gear before upshift" + gear);
    if ((gear >= 1) && (gear <= 6))
    {
      gear++;
      vTaskDelay(50); // delay(50);

      switch (gear)
      {
        case 2:
        case 3:
        case 4:
          upShift(0, currentLeverPosition, gear);
          break;
        case 5:
          upShift(15, currentLeverPosition, gear);
          break;
        case 6:
          select_five_to_fivetcc(currentLeverPosition, gear);
          break;
        default:
          gear = 6;
          gearlever->CompleteShiftRequest();
          return;
      }
      
      Serial.println("Current gear after upshift" + gear);
    }
  }

  // check for the down_shift
  else if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Drive && currentShiftRequest == VaLas_Controller::ShiftRequest::DownShift)
  {
    Serial.println("Downshift detected");
    Serial.println("Current gear before downshift" + gear);
    if ((gear >= 1) && (gear <= 6))
    {
      gear--;
      vTaskDelay(50); // delay(50);

      switch (gear)
      {
        case 2:
          downShift(20, currentLeverPosition, gear);
          break;
        case 1:
        case 3:
        case 4:
          downShift(0, currentLeverPosition, gear);
          break;
        case 5:
          select_fivetcc_to_five(currentLeverPosition, gear);
          break;
        default:
          gear = 1;
          gearlever->CompleteShiftRequest();
          return;
      }
      
      Serial.println("Current gear after downshift" + gear);
    }
  }
}

void ShiftControl::processLeverValues(VaLas_Controller::GearLeverPosition oldLeverPosition, VaLas_Controller::GearLeverPosition currentLeverPosition, int gear)
{
  if (currentLeverPosition == oldLeverPosition)
    return;

  // Start fresh from gear 2 if needed
  resetToGear2(currentLeverPosition, gear);

  // Log and display
  String printVar = displayHandlerPointer->ToString(currentLeverPosition, gear) + " selected";
  Serial.println(printVar);
}

void ShiftControl::resetToGear2(VaLas_Controller::GearLeverPosition currentLeverPosition, int gear)
{
  // Reset all shifting vars
  gearlever->Reset();
  gearlever->CompleteShiftRequest();

  //TODO: Do the actual reset to gear 2 or reset all pins/pwms?
  if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Park || currentLeverPosition == VaLas_Controller::GearLeverPosition::Neutral)
  {
    ledcWrite(pwmChannelsPointer->mpcChannel, (255/100*40)); //40%
    ledcWrite(pwmChannelsPointer->spcChannel, (255/100*33)); //33%
    ledcWrite(pwmChannelsPointer->y4Channel, (255/100*30)); //30%, Back to idle

    // 3-4 Shift solenoid is pulsed continuously while in Park and during selector lever movement (Garage Shifts).
    if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Park)
      ledcWrite(pwmChannelsPointer->y4Channel, 255);
    else
      ledcWrite(pwmChannelsPointer->y4Channel, 0);
  }
  else
  {
    ledcWrite(pwmChannelsPointer->y4Channel, 0);
    ledcWrite(pwmChannelsPointer->spcChannel, 0); // Set to 0 in D and R
  }
  
  // Reset only if we go to Reverse or Park, so we can continue in the same gear if going from N back to drive?
  if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Reverse || currentLeverPosition == VaLas_Controller::GearLeverPosition::Park)
    gear = 2;
}

//  * TCC is available in 2nd thru 5th gear, based on throttle position, fluid temp and vehicle speed
void ShiftControl::downShift(int customMpcAfterShift, VaLas_Controller::GearLeverPosition currentLeverPosition, int gear)
{
  *screenToDisplayValue = VaLas_Controller::DisplayScreen::Shifting;
  String screenVar = displayHandlerPointer->ToString(currentLeverPosition, gear);
  Serial.println("Downshift to " + screenVar);

  int gearPin = -1;
  if (gear == 1 || gear == 4)
    gearPin = y3Pin;
  else if (gear == 3)
    gearPin = y4Pin;
  else if (gear == 2)
    gearPin = y5Pin;
  else
    return; // Something went wrong

  ledcWrite(pwmChannelsPointer->mpcChannel, gearboxSettings[gear-1].DownshiftLinePressure);
  ledcWrite(pwmChannelsPointer->spcChannel, gearboxSettings[gear-1].DownshiftShiftPressure);
  digitalWrite(gearPin, HIGH);
  digitalWrite(tccPin, gearboxSettings[gear-1].DownshiftTorqueConverterLockup);

  if (gear == 2)
  {
    vTaskDelay(gearboxSettings[gear-1].DownshiftDelay);

    ledcWrite(pwmChannelsPointer->mpcChannel, (gearboxSettings[gear-1].DownshiftLinePressure /2));
    ledcWrite(pwmChannelsPointer->spcChannel, (gearboxSettings[gear-1].DownshiftShiftPressure /2));
    digitalWrite(gearPin, LOW);

    vTaskDelay(50); // delay(50);
  }
  else
  {
    vTaskDelay(gearboxSettings[gear-1].DownshiftDelay);
  }

  ledcWrite(pwmChannelsPointer->mpcChannel, customMpcAfterShift);
  ledcWrite(pwmChannelsPointer->spcChannel, 0);
  digitalWrite(gearPin, LOW);

  gearlever->CompleteShiftRequest();
  *screenToDisplayValue = VaLas_Controller::DisplayScreen::Main;
}

//  * TCC is available in 2nd thru 5th gear, based on throttle position, fluid temp and vehicle speed
void ShiftControl::upShift(int customMpcAfterShift, VaLas_Controller::GearLeverPosition currentLeverPosition, int gear)
{
  *screenToDisplayValue = VaLas_Controller::DisplayScreen::Shifting;
  String screenVar = displayHandlerPointer->ToString(currentLeverPosition, gear);
  Serial.println("Upshift to " + screenVar);

  int gearPin = -1;
  if (gear == 2 || gear == 5)
    gearPin = y3Pin;
  else if (gear == 4)
    gearPin = y4Pin;
  else if (gear == 3)
    gearPin = y5Pin;
  else
    return; // Something went wrong

  ledcWrite(pwmChannelsPointer->mpcChannel, gearboxSettings[gear-1].UpshiftLinePressure);
  ledcWrite(pwmChannelsPointer->spcChannel, gearboxSettings[gear-1].UpshiftShiftPressure);
  digitalWrite(gearPin, HIGH);
  digitalWrite(tccPin, gearboxSettings[gear-1].UpshiftTorqueConverterLockup);

  vTaskDelay(gearboxSettings[gear-1].UpshiftDelay);

  ledcWrite(pwmChannelsPointer->mpcChannel, customMpcAfterShift);
  ledcWrite(pwmChannelsPointer->spcChannel, 0);
  digitalWrite(gearPin, LOW);

  gearlever->CompleteShiftRequest();
  *screenToDisplayValue = VaLas_Controller::DisplayScreen::Main;
}

void ShiftControl::select_fivetcc_to_five(VaLas_Controller::GearLeverPosition currentLeverPosition, int gear)
// 5 OD -> 5
{
  *screenToDisplayValue = VaLas_Controller::DisplayScreen::Shifting;
  String screenVar = displayHandlerPointer->ToString(currentLeverPosition, gear);
  Serial.println("Downshift to " + screenVar);

  vTaskDelay(gearboxSettings[gear-1].DownshiftDelay);

  ledcWrite(pwmChannelsPointer->mpcChannel, 15);
  ledcWrite(pwmChannelsPointer->spcChannel, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, 0);

  gearlever->CompleteShiftRequest();
  *screenToDisplayValue = VaLas_Controller::DisplayScreen::Main;
}

void ShiftControl::select_five_to_fivetcc(VaLas_Controller::GearLeverPosition currentLeverPosition, int gear)
// 5 -> 5 OD
{
  *screenToDisplayValue = VaLas_Controller::DisplayScreen::Shifting;
  String screenVar = displayHandlerPointer->ToString(currentLeverPosition, gear);
  Serial.println("Downshift to " + screenVar);

  vTaskDelay(gearboxSettings[gear-1].UpshiftDelay);

  ledcWrite(pwmChannelsPointer->mpcChannel, 25);
  ledcWrite(pwmChannelsPointer->spcChannel, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, HIGH);

  gearlever->CompleteShiftRequest();
  *screenToDisplayValue = VaLas_Controller::DisplayScreen::Main;
}