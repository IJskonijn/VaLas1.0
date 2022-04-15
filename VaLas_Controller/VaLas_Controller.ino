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

// GEARLEVER SETTINGS
// 3.3V Pin on ESP32 + sensor pin 4
// Reads ranges:
// P   300-600
// R   1600-2000
// N   2100-2500
// D   3000-3400

#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include "VaLas_Controller.h"
#include "Sensors.h"

// 128x64 for 0.96" OLED
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

typedef enum GearLeverPosition
{
  Park,
  Reverse,
  Neutral,
  Drive,
  Unknown
};

typedef enum ShiftRequest
{
  NoShift,
  UpShift,
  DownShift
};

const int n2PulsesPerRev = 60;
const int n3PulsesPerRev = 60;

byte gear;
int up_shift = 0;
int down_shift = 0;
int old_upshift = 0;
int old_downshift = 0;

int pwmFreq = 1000;
int mpcChannel = 0; // Channel 0
int spcChannel = 1; // Channel 1
int tccChannel = 2; // Channel 2
int y4Channel = 3; // Channel 3

const char* stringToDisplayBuffer;

GearLeverPosition currentLeverPosition;
ShiftRequest currentShiftRequest;
Sensors sensors;

void setup()
{
  Serial.begin(9600); // open the serial port at 9600 bps:
  Serial.write("Begin program");
  Serial.write("\n");

  delay(500);

  u8g2.begin();
  displayOnScreen("VaLas");
  delay(1500);
  displayOnScreen("Ver. 1.1");
  delay(1500);

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

  currentLeverPosition = Unknown;
  currentShiftRequest = NoShift;
  gear = 2;
}

void loop()
{
  readGearLeverPosition();
  readSwitch();

  while (currentLeverPosition != Drive && currentShiftRequest != NoShift)
  {
    readGearLeverPosition();
    readSwitch();
    displayOnScreen("");
  }
  // While stopped, a switch as been pressed while in Drive

  // Check for the up_shift
  if (currentLeverPosition == Drive && currentShiftRequest == UpShift)
  {
    if ((gear >= 1) && (gear <= 6))
    {
      gear++;
      delay(100);

      switch (gear)
      {
      case 1:
        select_one();
        break;
      case 2:
        select_twoup();
        break;
      case 3:
        select_threeup();
        break;
      case 4:
        select_fourup();
        break;
      case 5:
        select_five();
        break;
      case 6:
        select_fivetcc();
        break;
      default:
        gear = 6;
        currentShiftRequest = NoShift;
        return;
      }
    }
  }

  // check for the down_shift
  else if (currentLeverPosition == Drive && currentShiftRequest == DownShift)
  {
    if ((gear >= 1) && (gear <= 6))
    {
      gear--;
      delay(100);

      switch (gear)
      {
      case 1:
        select_one();
        break;
      case 2:
        select_two();
        break;
      case 3:
        select_three();
        break;
      case 4:
        select_four();
        break;
      case 5:
        select_fivedown();
        break;
      case 6:
        select_fivetcc();
        break;
      default:
        gear = 1;
        currentShiftRequest = NoShift;
        return;
      }
    }
  }
}

void readSwitch()
{
  // Do nothing if not in Drive
  if (currentLeverPosition != Drive)
    return;

  // check upshift transition
  up_shift = digitalRead(upShiftPin);
  if ((up_shift == 0) && (old_upshift == 1))
  {
    currentShiftRequest = UpShift;
    delay(50);
    Serial.println("Upshift pressed");
  }
  old_upshift = up_shift;

  // check downshift transition
  down_shift = digitalRead(downShiftPin);
  if ((down_shift == 0) && (old_downshift == 1))
  {
    currentShiftRequest = DownShift;
    delay(50);
    Serial.println("Downshift pressed");
  }
  old_downshift = down_shift;
}

void readGearLeverPosition()
{
  int leverValue = analogRead(gearLeverPotPin);
  switch (leverValue)
  {
  case 300 ... 600:
    processLeverValue(Park);
    break;
  case 1400 ... 2000:
    processLeverValue(Reverse);
    break;
  case 2100 ... 2500:
    processLeverValue(Neutral);
    break;
  case 3000 ... 3400:
    processLeverValue(Drive);
    break;
  
  default: // I guess something went wrong...
    break;
  }

  delay(50);
}

void processLeverValue(GearLeverPosition position)
{
  if (currentLeverPosition == position)
    return;

  // Set new value and start fresh from gear 2
  currentLeverPosition = position;
  resetToGear2();

  // Log and display
  String printVar = ToString(position) + " selected";
  String screenVar = "" + printVar.substring(0,1) + " "; // Take first character. Example Park would print: - P -
  Serial.println(printVar);
  displayOnScreen(screenVar.c_str());

  if (position == Drive)
  {
    delay(500);
    displayOnScreen("D2");
  }
}

void resetToGear2()
{
  // Reset all shifting vars
  up_shift = 1;
  down_shift = 1;
  old_upshift = 0;
  old_downshift = 0;
  currentShiftRequest = NoShift;

  //TODO: Do the actual reset to gear 2 or reset all pins/pwms?
  if (currentLeverPosition == Park || currentLeverPosition == Neutral)
  {
    ledcWrite(mpcChannel, (255/100*40)); //40%
    ledcWrite(spcChannel, (255/100*33)); //33%
    ledcWrite(y4Channel, (255/100*30)); //30%, Back to idle

    // 3-4 Shift solenoid is pulsed continuously while in Park and during selector lever movement (Garage Shifts).
    if (currentLeverPosition == Park)
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
  if (currentLeverPosition == Reverse || currentLeverPosition == Park)
    gear = 2;
}

void select_one()
// 2 -> 1
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 1 -");

  ledcWrite(mpcChannel, 40);
  ledcWrite(spcChannel, 40);
  digitalWrite(y3Pin, HIGH);
  digitalWrite(tccPin, 0);

  delay(700);

  ledcWrite(mpcChannel, 0);
  ledcWrite(spcChannel, 0);
  digitalWrite(y3Pin, LOW);

  displayOnScreen("D1");

  currentShiftRequest = NoShift;
}

void select_two()
// 3 -> 2
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 2 -");

  ledcWrite(mpcChannel, 180);
  ledcWrite(spcChannel, 180);
  digitalWrite(tccPin, 0);

  delay(20);

  digitalWrite(y5Pin, HIGH);

  delay(600);

  ledcWrite(mpcChannel, 70);
  ledcWrite(spcChannel, 70);
  digitalWrite(y5Pin, LOW);

  delay(50);

  ledcWrite(mpcChannel, 20);
  ledcWrite(spcChannel, 0);

  displayOnScreen("D2");

  currentShiftRequest = NoShift;
}

void select_three()
// 4 -> 3
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 3 -");

  ledcWrite(mpcChannel, 140);
  ledcWrite(spcChannel, 140);
  digitalWrite(y4Pin, HIGH);
  digitalWrite(tccPin, 0);

  delay(600);

  ledcWrite(spcChannel, 0);
  ledcWrite(mpcChannel, 0);
  digitalWrite(y4Pin, LOW);
  digitalWrite(tccPin, 0);

  delay(50);

  displayOnScreen("D3");

  currentShiftRequest = NoShift;
}

void select_four()
// 5 -> 4
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 4 -");

  ledcWrite(mpcChannel, 140);
  ledcWrite(spcChannel, 140);
  digitalWrite(y3Pin, HIGH);
  digitalWrite(tccPin, 0);

  delay(600);

  ledcWrite(mpcChannel, 0);
  ledcWrite(spcChannel, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, 0);

  displayOnScreen("D4");

  currentShiftRequest = NoShift;
}

void select_five()
// 4 -> 5
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 5 +");

  ledcWrite(mpcChannel, 100);
  ledcWrite(spcChannel, 120);
  digitalWrite(y3Pin, HIGH);
  digitalWrite(tccPin, 0);

  delay(600);

  ledcWrite(mpcChannel, 15);
  ledcWrite(spcChannel, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, LOW);

  displayOnScreen("D5");

  currentShiftRequest = NoShift;
}

void select_fivetcc()
// 5 -> 5 OD
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 5tcc +");

  delay(400);

  ledcWrite(mpcChannel, 25);
  ledcWrite(spcChannel, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, HIGH);

  displayOnScreen("D5+ ");

  currentShiftRequest = NoShift;
}

void select_fivedown()
// 5 OD -> 5
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 5 -");

  delay(400);

  ledcWrite(mpcChannel, 15);
  ledcWrite(spcChannel, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, 0);

  displayOnScreen("D5");

  currentShiftRequest = NoShift;
}

void select_twoup()
// 1 -> 2
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 2 +");

  ledcWrite(mpcChannel, 80);
  ledcWrite(spcChannel, 90);
  digitalWrite(y3Pin, HIGH);
  digitalWrite(tccPin, 0);

  delay(600);

  ledcWrite(mpcChannel, 0);
  ledcWrite(spcChannel, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, 0);

  displayOnScreen("D2");

  currentShiftRequest = NoShift;
}

void select_threeup()
// 2 -> 3
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 3 +");

  ledcWrite(mpcChannel, 80);
  ledcWrite(spcChannel, 80);
  digitalWrite(y5Pin, HIGH);
  digitalWrite(tccPin, 0);

  delay(600);

  ledcWrite(mpcChannel, 0);
  ledcWrite(spcChannel, 0);
  digitalWrite(y5Pin, LOW);
  digitalWrite(tccPin, 0);

  displayOnScreen("D3");

  currentShiftRequest = NoShift;
}

void select_fourup()
// 3 -> 4
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 4 +");

  ledcWrite(mpcChannel, 90);
  ledcWrite(spcChannel, 100);
  digitalWrite(y4Pin, HIGH);
  digitalWrite(tccPin, 0);

  delay(1200);

  ledcWrite(mpcChannel, 0);
  ledcWrite(spcChannel, 0);
  digitalWrite(y4Pin, LOW);
  digitalWrite(tccPin, 0);

  displayOnScreen("D4");

  currentShiftRequest = NoShift;
}

void displayOnScreen(const char* stringToDisplay)
{
  if (strlen(stringToDisplay) > 0)
  {
    stringToDisplayBuffer = stringToDisplay;
  }

  u8g2.clearBuffer();

  // Draw gear      
  u8g2.setFont(u8g2_font_logisoso28_tr);
  u8g2.drawStr(1, 29, stringToDisplayBuffer);

  // Draw ATF temp
  String atfTemp;
  if (currentLeverPosition == Park || currentLeverPosition == Neutral)
    atfTemp = String("-");
  else
    atfTemp = String(sensors.ReadAtfTemp());

  String tempVar = "ATF: " + atfTemp;// + String(" C");
  u8g2.setFont(u8g2_font_logisoso18_tr);
  u8g2.drawStr(10, 65, tempVar.c_str());
  u8g2.sendBuffer();

  delay(150);
}

inline const String ToString(GearLeverPosition v)
{
  switch (v)
  {
    case Park:    return "Park";
    case Reverse: return "Reverse";
    case Neutral: return "Neutral";
    case Drive:   return "Drive";
    default:      return "Unknown";
  }
}