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

// GEAR SETTINGS
// PIN  1-2,4-5 SWITCH  ON/OFF
// PIN2 2-3     SWITCH  ON/OFF
// PIN3 3-4     SWITCH  ON/OFF
// PIN4 LINE PRESSURE   PWM 0-255 low=0
// PIN5 SWITCH PRESSURE PWM 0-255 low=0
// PIN6 TURBINE LOCK    PWM 0-255 low=0

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

int upShiftPin = 19;
int downShiftPin = 18;
int gearLeverPotPin = 4;    // Read potentiometer value to determine if in P, R, N, D

int y3Pin = 33;             // 1-2, 4-5 switch    shift      LOW/HIGH
int y4Pin = 35;             // 3-4 switch         shift      LOW/HIGH
int y5Pin = 37;             // 2-3 switch         shift      LOW/HIGH
int mpcPin = 39;            // Line pressure      MOD_PC     min-max 255-0
int spcPin = 41;            // Shift pressure     SHIFT_PC   min-max 255-0
int tccPin = 43;            // Turbine lockup     TCC        min-max 0-255
int atfTempPin = 40;        // ATF temp / P-N switch  

GearLeverPosition currentLeverPosition;
ShiftRequest currentShiftRequest;

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
  }
  // While stopped, a switch as been pressed when in Drive

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
  case 1600 ... 2000:
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
  String screenVar = " - " + printVar.substring(0,1) + " -"; // Take first character. Example Park would print: - P -
  Serial.println(printVar);
  displayOnScreen(screenVar.c_str());

  if (position == Drive)
  {
    delay(500);
    displayOnScreen("  D: 2");
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
    //analogWrite(mpcPin, (255/100*40)); //40%
    //analogWrite(spcPin, (255/100*33)); //33%
    //analogWrite(y4Pin, (255/100*30)); //30%, Back to idle

    // 3-4 Shift solenoid is pulsed continuously while in Park and during selector lever movement (Garage Shifts).
    // if (currentLeverPosition == Park)
    //   analogWrite(y4Pin, 255);
    // else
    //   analogWrite(y4Pin, 0);
  }
  else
  {
    //analogWrite(y4Pin, 0);
    //analogWrite(spcPin, 0); // Set to 0 in D and R
  }
  
  gear = 2;
}

// GEAR SETTINGS
// PIN  1-2,4-5 SWITCH  ON/OFF
// PIN2 2-3     SWITCH  ON/OFF
// PIN3 3-4     SWITCH  ON/OFF
// PIN4 LINE PRESSURE   PWM 0-255
// PIN5 SWITCH PRESSURE PWM 0-255
// PIN6 TURBINE LOCK    PWM 0-255

void select_one()
// 2 -> 1
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 1 -");

  //analogWrite(mpcPin, 40);
  //analogWrite(spcPin, 40);
  digitalWrite(y3Pin, HIGH);
  digitalWrite(tccPin, 0);

  delay(700);

  //analogWrite(mpcPin, 0);
  //analogWrite(spcPin, 0);
  digitalWrite(y3Pin, LOW);

  displayOnScreen("  D: 1");

  currentShiftRequest = NoShift;
}

void select_two()
// 3 -> 2
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 2 -");

  //analogWrite(mpcPin, 180);
  //analogWrite(spcPin, 180);
  digitalWrite(tccPin, 0);

  delay(20);

  digitalWrite(y5Pin, HIGH);

  delay(600);

  //analogWrite(mpcPin, 70);
  //analogWrite(spcPin, 70);
  digitalWrite(y5Pin, LOW);

  delay(50);

  //analogWrite(mpcPin, 20);
  //analogWrite(spcPin, 0);

  displayOnScreen("  D: 2");

  currentShiftRequest = NoShift;
}

void select_three()
// 4 -> 3
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 3 -");

  //analogWrite(mpcPin, 140);
  //analogWrite(spcPin, 140);
  digitalWrite(y4Pin, HIGH);
  digitalWrite(tccPin, 0);

  delay(600);

  //analogWrite(spcPin, 0);
  //analogWrite(mpcPin, 0);
  digitalWrite(y4Pin, LOW);
  digitalWrite(tccPin, 0);

  delay(50);

  displayOnScreen("  D: 3");

  currentShiftRequest = NoShift;
}

void select_four()
// 5 -> 4
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 4 -");

  //analogWrite(mpcPin, 140);
  //analogWrite(spcPin, 140);
  digitalWrite(y3Pin, HIGH);
  digitalWrite(tccPin, 0);

  delay(600);

  //analogWrite(mpcPin, 0);
  //analogWrite(spcPin, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, 0);

  displayOnScreen("  D: 4");

  currentShiftRequest = NoShift;
}

void select_five()
// 4 -> 5
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 5 +");

  //analogWrite(mpcPin, 100);
  //analogWrite(spcPin, 120);
  digitalWrite(y3Pin, HIGH);
  digitalWrite(tccPin, 0);

  delay(600);

  //analogWrite(mpcPin, 15);
  //analogWrite(spcPin, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, LOW);

  displayOnScreen("  D: 5");

  currentShiftRequest = NoShift;
}

void select_fivetcc()
// 5 -> 5 OD
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 5tcc +");

  delay(400);

  //analogWrite(mpcPin, 25);
  //analogWrite(spcPin, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, HIGH);

  displayOnScreen("  D: 5+");

  currentShiftRequest = NoShift;
}

void select_fivedown()
// 5 OD -> 5
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 5 -");

  delay(400);

  //analogWrite(mpcPin, 15);
  //analogWrite(spcPin, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, 0);

  displayOnScreen("  D: 5");

  currentShiftRequest = NoShift;
}

void select_twoup()
// 1 -> 2
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 2 +");

  //analogWrite(mpcPin, 80);
  //analogWrite(spcPin, 90);
  digitalWrite(y3Pin, HIGH);
  digitalWrite(tccPin, 0);

  delay(600);

  //analogWrite(mpcPin, 0);
  //analogWrite(spcPin, 0);
  digitalWrite(y3Pin, LOW);
  digitalWrite(tccPin, 0);

  displayOnScreen("  D: 2");

  currentShiftRequest = NoShift;
}

void select_threeup()
// 2 -> 3
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 3 +");

  //analogWrite(mpcPin, 80);
  //analogWrite(spcPin, 80);
  digitalWrite(y5Pin, HIGH);
  digitalWrite(tccPin, 0);

  delay(600);

  //analogWrite(mpcPin, 0);
  //analogWrite(spcPin, 0);
  digitalWrite(y5Pin, LOW);
  digitalWrite(tccPin, 0);

  displayOnScreen("  D: 3");

  currentShiftRequest = NoShift;
}

void select_fourup()
// 3 -> 4
{
  displayOnScreen(" SHIFT");
  Serial.println("Gear 4 +");

  //analogWrite(mpcPin, 90);
  //analogWrite(spcPin, 100);
  digitalWrite(y4Pin, HIGH);
  digitalWrite(tccPin, 0);

  delay(1200);

  //analogWrite(mpcPin, 0);
  //analogWrite(spcPin, 0);
  digitalWrite(y4Pin, LOW);
  digitalWrite(tccPin, 0);

  displayOnScreen("  D: 4");

  currentShiftRequest = NoShift;
}

void displayOnScreen(const char* stringToDisplay)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso28_tr);
  u8g2.drawStr(1, 29, stringToDisplay);
  u8g2.sendBuffer();
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

/// Optional stuff

void toggleHighIdle()
{
  // Set pwm signal to mechanical pump ELR pins
}

// Reading oil temp sensor / P-N switch (same input pin, see page 27: http://www.all-trans.by/assets/site/files/mercedes/722.6.1.pdf)
int readAtfTemp()
{
  uint8_t len = 14;
  int16_t atfMap[len][3] = {
      {2500, 846, 130},
      {2500, 843, 120},
      {2500, 840, 110},
      {2250, 835, 100},
      {2000, 830, 90},
      {2000, 825, 80},
      {1750, 819, 70},
      {1500, 811, 60},
      {1500, 800, 47},
      {1250, 798, 44},
      {1250, 783, 34},
      {1000, 778, 23},
      {750, 723, -10},
      {500, 652, -40},
  };
  byte idx = 0;
  static uint32_t m = millis() + 900;
  uint16_t adc = analogRead(atfTempPin);
  for (byte i = 0; i < len; i++)
  {
    if (adc >= atfMap[i][1])
    {
      idx = i;
      break;
    }
  }
  if (idx == 0)
  {
    return atfMap[0][2];
  }
  else if (idx > 0 && idx < len)
  {
    int16_t tempAbove = atfMap[idx - 1][2];
    int16_t temp = atfMap[idx][2];
    int16_t adcAbove = atfMap[idx - 1][1];
    int16_t curAdc = atfMap[idx][1];
    uint16_t res = map(adc, curAdc, adcAbove, temp, tempAbove);
    return res;
  }
  else
  {
    return atfMap[len - 1][2];
  }
}

int readRpm()
{  
  // Read stock OM606 rpm sensor here
  // Calculation: frequency / 144 (flywheel tooth) * 60 = RPM.

  // if (rpmSpeed)
  //   {
  //     // speed based on engine rpm
  //     vehicleSpeedRPM = tireCircumference * curRPM / (ratioFromGear(gear) * config.diffRatio) / 1000000 * 60;
  //     speedValue = vehicleSpeedRPM;
  //   } 
}

int readN2()
{
  // if (n2SpeedPulses >= n2PulsesPerRev)
  // {
  //   n2Speed = n2SpeedPulses / n2PulsesPerRev / elapsedTime * 1000 * 60; // there are 60 pulses in one rev and 60 seconds in minute, so this is quite simple
  //   n2SpeedPulses = 0;
  // }
  // else
  // {
  //   n2SpeedPulses = 0;
  //   n2Speed = 0;
  // }
}

int readN3()
{
  // if (n3SpeedPulses >= n3PulsesPerRev)
  // {
  //   n3Speed = n3SpeedPulses / n3PulsesPerRev / elapsedTime * 1000 * 60;
  //   n3SpeedPulses = 0;
  // }
  // else
  // {
  //   n3SpeedPulses = 0;
  //   n3Speed = 0;
  // }
}