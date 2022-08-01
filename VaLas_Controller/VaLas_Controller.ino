//VALAS Controller
//722.6 GEARBOX CONTROLLER
//SIMPLE MANUAL CONTROLLER WITH MINIMAL FEARURES FOR COMFORTABLE DRIVING
//BY TONI LASSILA & TEEMU VAHTOLA
//t6lato00@students.oamk.fi
//Version 1.1 by IJskonijn

//DOWNLOAD U8G2 TO YOUR ARDUINO LIBRARIRIES, FOR 0,91" / 0,96" OLED GEAR SCREEN!
//OTHERWISE ERASE ALL U8G2 COMMANDS

//LICENCE: CC BY-NC 3.0 https://creativecommons.org/licenses/by-nc/3.0/deed.en
//NOT FOR COMMERCIAL USE!

#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include "VaLas_Controller.h"
#include "ShiftConfig.h"
#include "DisplayHandler.h"
#include "Sensors.h"
#include "Gearlever.h"
#include "Gearlever_CAN.h"
#include "Gearlever_Modded.h"

int gear;

int pwmFreq = 1000;
int mpcChannel = 0; // Channel 0
int spcChannel = 1; // Channel 1
int tccChannel = 2; // Channel 2
int y4Channel = 3; // Channel 3

bool useCanBus = false;
VaLas_Controller::ShiftSetting gearboxSettings[6];

VaLas_Controller::GearLeverPosition oldLeverPosition;
VaLas_Controller::GearLeverPosition currentLeverPosition;
VaLas_Controller::ShiftRequest currentShiftRequest;

ShiftConfig shiftConfig;
Gearlever* gearlever;
Sensors sensors;
DisplayHandler displayHandler;

void setup()
{
  Serial.begin(9600); // open the serial port at 9600 bps:
  Serial.write("Begin program");
  Serial.write("\n");

  shiftConfig.LoadDefaultConfig(gearboxSettings, useCanBus);

  delay(500);

  displayHandler.DisplayStartupOnScreen();

  pinMode(upShiftPin, INPUT_PULLUP);
  pinMode(downShiftPin, INPUT_PULLUP);

  pinMode(y3Pin, OUTPUT);
  pinMode(y4Pin, OUTPUT);
  pinMode(y5Pin, OUTPUT);
  pinMode(mpcPin, OUTPUT);
  pinMode(spcPin, OUTPUT);
  pinMode(tccPin, OUTPUT);
  
  // Assign led pins to a channel
  ledcAttachPin(mpcPin, mpcChannel);
  ledcAttachPin(spcPin, spcChannel);
  ledcAttachPin(tccPin, tccChannel);
  ledcAttachPin(y4Pin, y4Channel);

  // ledcSetup(uint8_t channel, uint32_t frequency, uint8_t resolution_bits);
  ledcSetup(mpcChannel, pwmFreq, 8); // PWM, 8-bit resolution > 0-255
  ledcSetup(spcChannel, pwmFreq, 8);
  ledcSetup(tccChannel, pwmFreq, 8);
  ledcSetup(y4Channel, pwmFreq, 8);

  // Set all the ELR inputs and outputs
  pinMode(elrTogglePin, INPUT_PULLUP);
  pinMode(elrPwmPin, OUTPUT);
  ledcAttachPin(elrPwmPin, elrChannel);
  ledcSetup(elrChannel, elrPwmFreq, 8);

  if (useCanBus)
    gearlever = new Gearlever_CAN();
  else
    gearlever = new Gearlever_Modded();

  currentLeverPosition = VaLas_Controller::GearLeverPosition::Unknown;
  currentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;
  gear = 2;
}

void loop()
{
  processLeverValues();

  while (currentLeverPosition != VaLas_Controller::GearLeverPosition::Drive && currentShiftRequest != VaLas_Controller::ShiftRequest::NoShift)
  {
    processLeverValues();
    displayHandler.DisplayOnScreen("");
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

void processLeverValues()
{
  oldLeverPosition = currentLeverPosition;
  gearlever->ReadGearLever(currentShiftRequest, currentLeverPosition);

  if (currentLeverPosition == oldLeverPosition)
    return;

  // Start fresh from gear 2 if needed
  resetToGear2();

  // Log and display
  String printVar = displayHandler.ToString(currentLeverPosition) + " selected";
  String screenVar = "" + printVar.substring(0,1); // Take first character. Example Park would print: - P -
  Serial.println(printVar);
  displayHandler.DisplayOnScreen(screenVar.c_str());

  if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Drive)
  {
    delay(500);
    String intToString = String(gear);
    String screenVarD = "" + screenVar + intToString;
    displayHandler.DisplayOnScreen(screenVarD.c_str());
  }
}

void resetToGear2()
{
  // Reset all shifting vars
  gearlever->Reset();
  currentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;

  //TODO: Do the actual reset to gear 2 or reset all pins/pwms?
  if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Park || currentLeverPosition == VaLas_Controller::GearLeverPosition::Neutral)
  {
    ledcWrite(mpcChannel, (255/100*40)); //40%
    ledcWrite(spcChannel, (255/100*33)); //33%
    ledcWrite(y4Channel, (255/100*30)); //30%, Back to idle

    // 3-4 Shift solenoid is pulsed continuously while in Park and during selector lever movement (Garage Shifts).
    if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Park)
      ledcWrite(y4Channel, 255);
    else
      ledcWrite(y4Channel, 0);
  }
  else
  {
    ledcWrite(y4Channel, 0);
    ledcWrite(spcChannel, 0); // Set to 0 in D and R
  }
  
  // Reset only if we go to Reverse or Park, so we can continue in the same gear if going from N back to drive?
  if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Reverse || currentLeverPosition == VaLas_Controller::GearLeverPosition::Park)
    gear = 2;
}


//  * TCC is available in 2nd thru 5th gear, based on throttle position, fluid temp and vehicle speed
void downShift(int customMpcAfterShift)
{
  displayHandler.DisplayOnScreen(" SHIFT");
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

  ledcWrite(mpcChannel, gearboxSettings[gear-1].DownshiftLinePressure);
  ledcWrite(spcChannel, gearboxSettings[gear-1].DownshiftShiftPressure);
  digitalWrite(gearPin, HIGH);
  digitalWrite(tccPin, gearboxSettings[gear-1].DownshiftTorqueConverterLockup);

  if (gear == 2)
  {
    delay(gearboxSettings[gear-1].DownshiftDelay);

    ledcWrite(mpcChannel, (gearboxSettings[gear-1].DownshiftLinePressure /2));
    ledcWrite(spcChannel, (gearboxSettings[gear-1].DownshiftShiftPressure /2));
    digitalWrite(gearPin, LOW);

    delay(50);
  }
  else
  {
    delay(gearboxSettings[gear-1].DownshiftDelay);
  }

  ledcWrite(mpcChannel, customMpcAfterShift);
  ledcWrite(spcChannel, 0);
  digitalWrite(gearPin, LOW);

  displayHandler.DisplayOnScreen(screenVarD.c_str());
  currentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;
}

//  * TCC is available in 2nd thru 5th gear, based on throttle position, fluid temp and vehicle speed
void upShift(int customMpcAfterShift)
{
  displayHandler.DisplayOnScreen(" SHIFT");
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

  ledcWrite(mpcChannel, gearboxSettings[gear-1].UpshiftLinePressure);
  ledcWrite(spcChannel, gearboxSettings[gear-1].UpshiftShiftPressure);
  digitalWrite(gearPin, HIGH);
  digitalWrite(tccPin, gearboxSettings[gear-1].UpshiftTorqueConverterLockup);

  delay(gearboxSettings[gear-1].UpshiftDelay);

  ledcWrite(mpcChannel, customMpcAfterShift);
  ledcWrite(spcChannel, 0);
  digitalWrite(gearPin, LOW);

  displayHandler.DisplayOnScreen(screenVarD.c_str());
  currentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;
}

void select_fivetcc_to_five()
// 5 OD -> 5
{
  displayHandler.DisplayOnScreen(" SHIFT");
  Serial.println("Gear 5 -");

  delay(400);

  ledcWrite(mpcChannel, 15);
  ledcWrite(spcChannel, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, 0);

  displayHandler.DisplayOnScreen("D5");

  currentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;
}

void select_five_to_fivetcc()
// 5 -> 5 OD
{
  displayHandler.DisplayOnScreen(" SHIFT");
  Serial.println("Gear 5tcc +");

  delay(400);

  ledcWrite(mpcChannel, 25);
  ledcWrite(spcChannel, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, HIGH);

  displayHandler.DisplayOnScreen("D5+ ");

  currentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;
}

