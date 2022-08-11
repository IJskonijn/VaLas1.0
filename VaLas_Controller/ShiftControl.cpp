#include <Arduino.h>
#include "ShiftControl.h"
#include "ShiftConfig.h"
#include "VaLas_Controller.h"
#include "displayHandler.h"
#include "Sensors.h"
#include "Gearlever.h"
#include "Gearlever_CAN.h"
#include "Gearlever_Modded.h"

ShiftConfig shiftConfig;
VaLas_Controller::PwmChannels* pwmChannelsPointer;
DisplayHandler* displayHandlerPointer;
Gearlever* gearlever;

bool useCanBus = false;
VaLas_Controller::ShiftSetting gearboxSettings[6];

VaLas_Controller::GearLeverPosition oldLeverPosition;
VaLas_Controller::GearLeverPosition currentLeverPosition;
VaLas_Controller::ShiftRequest currentShiftRequest;

int gear;


void ShiftControl::Init(DisplayHandler* displayHandlerPtr, VaLas_Controller::PwmChannels* pwmChannelsPtr)
{
  displayHandlerPointer = displayHandlerPtr;
  pwmChannelsPointer = pwmChannelsPtr;
  
  if (useCanBus)
    gearlever = new Gearlever_CAN();
  else
    gearlever = new Gearlever_Modded();

  currentLeverPosition = VaLas_Controller::GearLeverPosition::Unknown;
  currentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;
  gear = 2;

  shiftConfig.LoadDefaultConfig(gearboxSettings, useCanBus);
}

void ShiftControl::DoTheStuff()
{
  processLeverValues();

  while (currentLeverPosition != VaLas_Controller::GearLeverPosition::Drive && currentShiftRequest != VaLas_Controller::ShiftRequest::NoShift)
  {
    processLeverValues();
    displayHandlerPointer->DisplayOnScreen("");
  }
  // While stopped, a switch as been pressed while in Drive

  // Check for the up_shift
  if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Drive && currentShiftRequest == VaLas_Controller::ShiftRequest::UpShift)
  {
    if ((gear >= 1) && (gear <= 6))
    {
      gear++;
      delay(100);

      switch (gear)
      {
      case 2:
      case 3:
      case 4:
        upShift(0);
        break;
      case 5:
        upShift(15);
        break;
      case 6:
        select_five_to_fivetcc();
        break;
      default:
        gear = 6;
        currentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;
        return;
      }
    }
  }

  // check for the down_shift
  else if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Drive && currentShiftRequest == VaLas_Controller::ShiftRequest::DownShift)
  {
    if ((gear >= 1) && (gear <= 6))
    {
      gear--;
      delay(100);

      switch (gear)
      {
        case 2:
          downShift(20);
          break;
        case 1:
        case 3:
        case 4:
          downShift(0);
          break;
        case 5:
          select_fivetcc_to_five();
          break;
        default:
          gear = 1;
          currentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;
          return;
      }
    }
  }
}

void ShiftControl::processLeverValues()
{
  oldLeverPosition = currentLeverPosition;
  gearlever->ReadGearLever(currentShiftRequest, currentLeverPosition);

  if (currentLeverPosition == oldLeverPosition)
    return;

  // Start fresh from gear 2 if needed
  resetToGear2();

  // Log and display
  String printVar = displayHandlerPointer->ToString(currentLeverPosition) + " selected";
  String screenVar = "" + printVar.substring(0,1); // Take first character. Example Park would print: - P -
  Serial.println(printVar);
  displayHandlerPointer->DisplayOnScreen(screenVar.c_str());

  if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Drive)
  {
    delay(500);
    String intToString = String(gear);
    String screenVarD = "" + screenVar + intToString;
    displayHandlerPointer->DisplayOnScreen(screenVarD.c_str());
  }
}

void ShiftControl::resetToGear2()
{
  // Reset all shifting vars
  gearlever->Reset();
  currentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;

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
void ShiftControl::downShift(int customMpcAfterShift)
{
  displayHandlerPointer->DisplayOnScreen(" SHIFT");
  String intToString = String(gear);
  String screenVarD = "D" + intToString;
  Serial.println("Downshift to " + screenVarD);

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
    delay(gearboxSettings[gear-1].DownshiftDelay);

    ledcWrite(pwmChannelsPointer->mpcChannel, (gearboxSettings[gear-1].DownshiftLinePressure /2));
    ledcWrite(pwmChannelsPointer->spcChannel, (gearboxSettings[gear-1].DownshiftShiftPressure /2));
    digitalWrite(gearPin, LOW);

    delay(50);
  }
  else
  {
    delay(gearboxSettings[gear-1].DownshiftDelay);
  }

  ledcWrite(pwmChannelsPointer->mpcChannel, customMpcAfterShift);
  ledcWrite(pwmChannelsPointer->spcChannel, 0);
  digitalWrite(gearPin, LOW);

  displayHandlerPointer->DisplayOnScreen(screenVarD.c_str());
  currentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;
}

//  * TCC is available in 2nd thru 5th gear, based on throttle position, fluid temp and vehicle speed
void ShiftControl::upShift(int customMpcAfterShift)
{
  displayHandlerPointer->DisplayOnScreen(" SHIFT");
  String intToString = String(gear);
  String screenVarD = "D" + intToString;
  Serial.println("Upshift to " + screenVarD);

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

  delay(gearboxSettings[gear-1].UpshiftDelay);

  ledcWrite(pwmChannelsPointer->mpcChannel, customMpcAfterShift);
  ledcWrite(pwmChannelsPointer->spcChannel, 0);
  digitalWrite(gearPin, LOW);

  displayHandlerPointer->DisplayOnScreen(screenVarD.c_str());
  currentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;
}

void ShiftControl::select_fivetcc_to_five()
// 5 OD -> 5
{
  displayHandlerPointer->DisplayOnScreen(" SHIFT");
  Serial.println("Gear 5 -");

  delay(400);

  ledcWrite(pwmChannelsPointer->mpcChannel, 15);
  ledcWrite(pwmChannelsPointer->spcChannel, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, 0);

  displayHandlerPointer->DisplayOnScreen("D5");

  currentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;
}

void ShiftControl::select_five_to_fivetcc()
// 5 -> 5 OD
{
  displayHandlerPointer->DisplayOnScreen(" SHIFT");
  Serial.println("Gear 5tcc +");

  delay(400);

  ledcWrite(pwmChannelsPointer->mpcChannel, 25);
  ledcWrite(pwmChannelsPointer->spcChannel, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, HIGH);

  displayHandlerPointer->DisplayOnScreen("D5+ ");

  currentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;
}