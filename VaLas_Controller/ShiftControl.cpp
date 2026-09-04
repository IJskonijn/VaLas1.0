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
int* throttlePositionPointer;
bool* useThrottlePositionPointer;
VaLas_Controller::ThrottleSettings* throttleSettingsPointer;


void ShiftControl::init(DisplayHandler* displayHandlerPtr, VaLas_Controller::PwmChannels* pwmChannelsPtr, Gearlever* gearLeverPtr, 
  VaLas_Controller::DisplayScreen* screenToDisplayPtr, VaLas_Controller::ShiftSetting* gearboxSettingsPtr)
{
  Serial.println("Init ShiftControl");
  displayHandlerPointer = displayHandlerPtr;
  pwmChannelsPointer = pwmChannelsPtr;
  gearlever = gearLeverPtr;
  screenToDisplayValue = screenToDisplayPtr;
  gearboxSettings = gearboxSettingsPtr;
  throttlePositionPointer = nullptr;
  useThrottlePositionPointer = nullptr;
  throttleSettingsPointer = nullptr;
}

void ShiftControl::execute(void * parameter)
{ 
  TaskStructs::ShiftControlParameters *parameters = (TaskStructs::ShiftControlParameters*) parameter;
  int* gear = parameters->gearPtr;
  throttlePositionPointer = parameters->throttlePositionPtr;
  useThrottlePositionPointer = parameters->useThrottlePositionPtr;
  throttleSettingsPointer = parameters->throttleSettingsPtr;
  //VaLas_Controller::ShiftSetting* gearboxSettings = parameters->shiftSettings;
  VaLas_Controller::GearLeverPosition oldLeverPosition = *(parameters->oldLeverPositionPtr);
  VaLas_Controller::GearLeverPosition currentLeverPosition = *(parameters->currentLeverPositionPtr);
  VaLas_Controller::ShiftRequest currentShiftRequest = *(parameters->currentShiftRequestPtr);

  processLeverValues(oldLeverPosition, currentLeverPosition, gear);

  // Only process shift requests in Drive or Reverse
  if ((currentLeverPosition != VaLas_Controller::GearLeverPosition::Drive && currentLeverPosition != VaLas_Controller::GearLeverPosition::Reverse) 
      || currentShiftRequest == VaLas_Controller::ShiftRequest::NoShift)
  {
    //Serial.println("No shiftrequest");
    return; // Nothing to do if there is no shiftrequest 
  }

  // Check for the up_shift in Drive
  if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Drive && currentShiftRequest == VaLas_Controller::ShiftRequest::UpShift)
  {
    Serial.println("Upshift detected");
    Serial.println("Current gear before upshift " + String(*gear));
    if ((*gear >= 1) && (*gear <= 5))
    {
      (*gear)++;
      //vTaskDelay(50); // delay(50);
      Serial.println("Upshifting to " + String(*gear));

      switch (*gear)
      {
        case 2:
        case 3:
        case 4:
          upShift(0, currentLeverPosition, *gear);
          break;
        case 5:
          upShift(15, currentLeverPosition, *gear);
          break;
        case 6:
          select_five_to_fivetcc(currentLeverPosition, *gear);
          break;
        default:
          *gear = 6;
          return;
      }
    }

    gearlever->CompleteShiftRequest();
    *screenToDisplayValue = VaLas_Controller::DisplayScreen::Main;
    
    Serial.println("Current gear after upshift" + String(*gear));
  }

  // check for the down_shift
  else if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Drive && currentShiftRequest == VaLas_Controller::ShiftRequest::DownShift)
  {
    Serial.println("Downshift detected");
    Serial.println("Current gear before downshift" + String(*gear));
    if ((*gear >= 2) && (*gear <= 6))
    {
      (*gear)--;
      //vTaskDelay(50); // delay(50);
      Serial.println("Downshifting to " + String(*gear));

      switch (*gear)
      {
        case 2:
          downShift(20, currentLeverPosition, *gear);
          break;
        case 1:
        case 3:
        case 4:
          downShift(0, currentLeverPosition, *gear);
          break;
        case 5:
          select_fivetcc_to_five(currentLeverPosition, *gear);
          break;
        default:
          *gear = 1;
          return;
      }
    }

    gearlever->CompleteShiftRequest();
    *screenToDisplayValue = VaLas_Controller::DisplayScreen::Main;
    
    Serial.println("Current gear after downshift" + String(*gear));
  }

  // Check for upshift in Reverse (R1 -> R2)
  else if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Reverse && currentShiftRequest == VaLas_Controller::ShiftRequest::UpShift)
  {
    Serial.println("Reverse upshift detected");
    Serial.println("Current gear before upshift " + String(*gear));
    
    if (*gear == 1)
    {
      *gear = 2;
      Serial.println("Upshifting to R2");
      
      // R1 -> R2 uses same shift as forward 1->2
      upShift(0, currentLeverPosition, *gear);
      
      Serial.println("Current gear after upshift " + String(*gear));
    }

    gearlever->CompleteShiftRequest();
    *screenToDisplayValue = VaLas_Controller::DisplayScreen::Main;
  }

  // Check for downshift in Reverse (R2 -> R1)
  else if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Reverse && currentShiftRequest == VaLas_Controller::ShiftRequest::DownShift)
  {
    Serial.println("Reverse downshift detected");
    Serial.println("Current gear before downshift " + String(*gear));
    
    if (*gear == 2)
    {
      *gear = 1;
      Serial.println("Downshifting to R1");
      
      // R2 -> R1 uses same shift as forward 2->1
      downShift(20, currentLeverPosition, *gear);
      
      Serial.println("Current gear after downshift " + String(*gear));
    }

    gearlever->CompleteShiftRequest();
    *screenToDisplayValue = VaLas_Controller::DisplayScreen::Main;
  }
}

void ShiftControl::processLeverValues(VaLas_Controller::GearLeverPosition oldLeverPosition, VaLas_Controller::GearLeverPosition currentLeverPosition, int* gear)
{
  if (currentLeverPosition == oldLeverPosition)
    return;

  // Start fresh from gear 2 if needed
  resetToGear2(currentLeverPosition, gear);

  // Log and display
  String printVar = displayHandlerPointer->ToString(currentLeverPosition, *gear) + " selected";
  Serial.println(printVar);
}

void ShiftControl::resetToGear2(VaLas_Controller::GearLeverPosition currentLeverPosition, int* gear)
{
  // Reset all shifting vars
  gearlever->Reset();
  gearlever->CompleteShiftRequest();

  //TODO: Do the actual reset to gear 2 or reset all pins/pwms?
  if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Park || currentLeverPosition == VaLas_Controller::GearLeverPosition::Neutral)
  {
    ledcWrite(pwmChannelsPointer->mpcChannel, (255 * 40) / 100); //40%
    ledcWrite(pwmChannelsPointer->spcChannel, (255 * 33) / 100); //33%
    digitalWrite(y4Pin, LOW); // Back to idle

    // 3-4 Shift solenoid is pulsed continuously while in Park and during selector lever movement (Garage Shifts).
    // New info, 3-4 solenoid is not actually pulsing in original EGS
    // if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Park)
    //   ledcWrite(pwmChannelsPointer->y4Channel, 255);
    // else
    //   ledcWrite(pwmChannelsPointer->y4Channel, 0);
  }
  else
  {
    digitalWrite(y4Pin, LOW);
    ledcWrite(pwmChannelsPointer->spcChannel, 0); // Set to 0 in D and R
  }
  
  // Reset only if we go to Reverse or Park, so we can continue in the same gear if going from N back to drive?
  if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Reverse || currentLeverPosition == VaLas_Controller::GearLeverPosition::Park)
    *gear = 2;
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

  int throttlePosition = throttlePositionPointer ? *throttlePositionPointer : 100;
  int linePressure = scalePressure(gearboxSettings[gear].DownshiftLinePressure, throttlePosition);
  int shiftPressure = scalePressure(gearboxSettings[gear].DownshiftShiftPressure, throttlePosition);
  int shiftDelay = gearboxSettings[gear].DownshiftDelay + getThrottleDelayMs(throttlePosition);
  ledcWrite(pwmChannelsPointer->mpcChannel, linePressure);
  ledcWrite(pwmChannelsPointer->spcChannel, shiftPressure);
  digitalWrite(gearPin, HIGH);
  ledcWrite(pwmChannelsPointer->tccChannel, gearboxSettings[gear].DownshiftTorqueConverterLockup);

  if (gear == 2)
  {
    vTaskDelay(shiftDelay);

    ledcWrite(pwmChannelsPointer->mpcChannel, linePressure / 2);
    ledcWrite(pwmChannelsPointer->spcChannel, shiftPressure / 2);
    digitalWrite(gearPin, LOW);

    vTaskDelay(50); // delay(50);
  }
  else
  {
    vTaskDelay(shiftDelay);
  }

  ledcWrite(pwmChannelsPointer->mpcChannel, scalePressure(customMpcAfterShift, throttlePosition));
  ledcWrite(pwmChannelsPointer->spcChannel, 0);
  digitalWrite(gearPin, LOW);
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

  int throttlePosition = throttlePositionPointer ? *throttlePositionPointer : 100;
  int linePressure = scalePressure(gearboxSettings[gear - 2].UpshiftLinePressure, throttlePosition);
  int shiftPressure = scalePressure(gearboxSettings[gear - 2].UpshiftShiftPressure, throttlePosition);
  int shiftDelay = gearboxSettings[gear - 2].UpshiftDelay + getThrottleDelayMs(throttlePosition);
  ledcWrite(pwmChannelsPointer->mpcChannel, linePressure);
  ledcWrite(pwmChannelsPointer->spcChannel, shiftPressure);
  digitalWrite(gearPin, HIGH);
  ledcWrite(pwmChannelsPointer->tccChannel, gearboxSettings[gear - 2].UpshiftTorqueConverterLockup);

  vTaskDelay(shiftDelay);

  ledcWrite(pwmChannelsPointer->mpcChannel, scalePressure(customMpcAfterShift, throttlePosition));
  ledcWrite(pwmChannelsPointer->spcChannel, 0);
  digitalWrite(gearPin, LOW);
}

void ShiftControl::select_fivetcc_to_five(VaLas_Controller::GearLeverPosition currentLeverPosition, int gear)
// 5 OD -> 5
{
  *screenToDisplayValue = VaLas_Controller::DisplayScreen::Shifting;
  String screenVar = displayHandlerPointer->ToString(currentLeverPosition, gear);
  Serial.println("Downshift to " + screenVar);

  int throttlePosition = throttlePositionPointer ? *throttlePositionPointer : 100;
  vTaskDelay(gearboxSettings[gear].DownshiftDelay + getThrottleDelayMs(throttlePosition));

  ledcWrite(pwmChannelsPointer->mpcChannel, scalePressure(15, throttlePosition));
  ledcWrite(pwmChannelsPointer->spcChannel, 0);
  digitalWrite(y3Pin, LOW);
  ledcWrite(pwmChannelsPointer->tccChannel, 0);
}

void ShiftControl::select_five_to_fivetcc(VaLas_Controller::GearLeverPosition currentLeverPosition, int gear)
// 5 -> 5 OD
{
  *screenToDisplayValue = VaLas_Controller::DisplayScreen::Shifting;
  String screenVar = displayHandlerPointer->ToString(currentLeverPosition, gear);
  Serial.println("Downshift to " + screenVar);

  int throttlePosition = throttlePositionPointer ? *throttlePositionPointer : 100;
  vTaskDelay(gearboxSettings[gear - 2].UpshiftDelay + getThrottleDelayMs(throttlePosition));

  ledcWrite(pwmChannelsPointer->mpcChannel, scalePressure(25, throttlePosition));
  ledcWrite(pwmChannelsPointer->spcChannel, 0);
  digitalWrite(y3Pin, LOW);
  ledcWrite(pwmChannelsPointer->tccChannel, (255 * 95) / 100); //95% for torque converter lockup with some slip for comfort
}

int ShiftControl::getThrottlePressurePercent(int throttlePosition)
{
  if (!useThrottlePositionPointer || !*useThrottlePositionPointer || !throttleSettingsPointer)
    return 100;

  if (throttlePosition < 50)
    return throttleSettingsPointer->lowThrottlePressurePercent;
  if (throttlePosition < 80)
    return throttleSettingsPointer->mediumThrottlePressurePercent;
  return throttleSettingsPointer->highThrottlePressurePercent;
}

int ShiftControl::getThrottleDelayMs(int throttlePosition)
{
  if (!useThrottlePositionPointer || !*useThrottlePositionPointer || !throttleSettingsPointer)
    return 0;

  if (throttlePosition < 50)
    return constrain(throttleSettingsPointer->lowThrottleDelayMs, 0, 500);
  if (throttlePosition < 80)
    return constrain(throttleSettingsPointer->mediumThrottleDelayMs, 0, 500);
  return constrain(throttleSettingsPointer->highThrottleDelayMs, 0, 500);
}

int ShiftControl::scalePressure(int pressure, int throttlePosition)
{
  pressure = constrain(pressure, 0, 255);
  int pressurePercent = constrain(getThrottlePressurePercent(throttlePosition), 0, 100);
  return constrain((pressure * pressurePercent) / 100, 0, 255);
}