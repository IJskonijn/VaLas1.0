//VALAS Controller
//722.6 GEARBOX CONTROLLER
//SIMPLE MANUAL CONTROLLER WITH MINIMAL FEARURES FOR COMFORTABLE DRIVING
//BY TONI LASSILA & TEEMU VAHTOLA
//t6lato00@students.oamk.fi
//Version 1.1 by IJskonijn

//DOWNLOAD U8G2 TO YOUR ARDUINO LIBRARIRIES, FOR 0,91" OLED GEAR SCREEN!
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

#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);

const int inpin[2] = { 53, 50 };
byte gear;

//255/100*40=102
//255/100*33=84

int up_shift = 1;
int down_shift = 1;
int old_upshift = 1;
int old_downshift = 1;

int gear1And2Plus4And5Pin = 33;   // 1-2, 4-5 switch    shift      LOW/HIGH
int gear2And3Pin = 35;            // 2-3 switch         shift      LOW/HIGH
int gear3And4Pin = 37;            // 3-4 switch         shift      LOW/HIGH
int linePressurePin = 39;         // Line pressure      MOD_PC     min-max 255-0
int shiftPressurePin = 41;        // Shift pressure     SHIFT_PC   min-max 255-0
int turbineLockupPin = 43;        // Turbine lockup     TCC        min-max 0-255

boolean status = 0;

void setup()
{
  Serial.begin(9600); // open the serial port at 9600 bps:
  Serial.write("Begin program");
  Serial.write("\n");

  delay(1500);

  u8g2.begin();
  displayOnScreen("VaLas");
  delay(1800);
  displayOnScreen("Ver. 1.1");
  delay(1500);
  displayOnScreen("GEAR 2");

  pinMode(inpin[0], INPUT_PULLUP);
  pinMode(inpin[1], INPUT_PULLUP);

  pinMode(gear1And2Plus4And5Pin, OUTPUT);
  pinMode(gear2And3Pin, OUTPUT);
  pinMode(gear3And4Pin, OUTPUT);
  pinMode(linePressurePin, OUTPUT);
  pinMode(shiftPressurePin, OUTPUT);
  pinMode(turbineLockupPin, OUTPUT);

  gear = 2;
}

void loop()
{
  readswitch();

  while (status == 0)
  {
    readswitch();
  }
  // While stopped, a switch as been pressed

  // Check for the up_shift
  if ((up_shift == 0) && (status == 1))
  {
    gear++;
    delay(100);

    if ((gear >= 1) && (gear <= 6))
    {
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
      }
    }
    else
    {
      gear = 6;
    }
  }

  // check for the down_shift
  if ((down_shift == 0) && (status == 1))
  {
    gear--;
    delay(100);

    if ((gear >= 1) && (gear <= 6))
    {
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
      }
    }
    else
    {
      gear = 1;
    }
  }
}

void readswitch()
{
  up_shift = digitalRead(inpin[0]);

  // check upshift transition
  if ((up_shift == 0) && (old_upshift == 1))
  {
    status = 1;
    delay(50);
  }
  old_upshift = up_shift;

  down_shift = digitalRead(inpin[1]);

  // check downshift transition
  if ((down_shift == 0) && (old_downshift == 1))
  {
    status = 1;
    delay(50);
  }
  old_downshift = down_shift;
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
  displayOnScreen("SHIFT");
  Serial.println("1");

  analogWrite(linePressurePin, 40);
  analogWrite(shiftPressurePin, 40);
  digitalWrite(gear1And2Plus4And5Pin, HIGH);
  digitalWrite(turbineLockupPin, 0);

  delay(700);

  analogWrite(linePressurePin, 0);
  analogWrite(shiftPressurePin, 0);
  digitalWrite(gear1And2Plus4And5Pin, LOW);

  displayOnScreen("GEAR 1");

  status = 0;
}

void select_two()
// 3 -> 2
{
  displayOnScreen("SHIFT");
  Serial.println("2");

  analogWrite(linePressurePin, 180);
  analogWrite(shiftPressurePin, 180);
  digitalWrite(turbineLockupPin, 0);

  delay(20);

  digitalWrite(gear2And3Pin, HIGH);

  delay(600);

  analogWrite(linePressurePin, 70);
  analogWrite(shiftPressurePin, 70);
  digitalWrite(gear2And3Pin, LOW);

  delay(50);

  analogWrite(linePressurePin, 20);
  analogWrite(shiftPressurePin, 0);

  displayOnScreen("GEAR 2");

  status = 0;
}

void select_three()
// 4 -> 3
{
  displayOnScreen("SHIFT");
  Serial.println("3");

  analogWrite(linePressurePin, 140);
  analogWrite(shiftPressurePin, 140);
  digitalWrite(gear3And4Pin, HIGH);
  digitalWrite(turbineLockupPin, 0);

  delay(600);

  analogWrite(shiftPressurePin, 0);
  analogWrite(linePressurePin, 0);
  digitalWrite(gear3And4Pin, LOW);
  digitalWrite(turbineLockupPin, 0);

  delay(50);

  displayOnScreen("GEAR 3");

  status = 0;
}

void select_four()
// 5 -> 4
{
  displayOnScreen("SHIFT");
  Serial.println("4");

  analogWrite(linePressurePin, 140);
  analogWrite(shiftPressurePin, 140);
  digitalWrite(gear1And2Plus4And5Pin, HIGH);
  digitalWrite(turbineLockupPin, 0);

  delay(600);

  analogWrite(linePressurePin, 0);
  analogWrite(shiftPressurePin, 0);
  digitalWrite(gear1And2Plus4And5Pin, LOW);
  digitalWrite(turbineLockupPin, 0);

  displayOnScreen("GEAR 4");

  status = 0;
}

void select_five()
// 4 -> 5
{
  displayOnScreen("SHIFT");
  Serial.println("5");

  analogWrite(linePressurePin, 100);
  analogWrite(shiftPressurePin, 120);
  digitalWrite(gear1And2Plus4And5Pin, HIGH);
  digitalWrite(turbineLockupPin, 0);

  delay(600);

  analogWrite(linePressurePin, 15);
  analogWrite(shiftPressurePin, 0);
  digitalWrite(gear1And2Plus4And5Pin, LOW);
  digitalWrite(turbineLockupPin, LOW);

  displayOnScreen("GEAR 5");

  status = 0;
}

void select_fivetcc()
// 5 -> 5 OD
{
  displayOnScreen("SHIFT");
  Serial.println("5tcc");

  delay(400);

  analogWrite(linePressurePin, 25);
  analogWrite(shiftPressurePin, 0);
  digitalWrite(gear1And2Plus4And5Pin, LOW);
  digitalWrite(turbineLockupPin, HIGH);

  displayOnScreen("GEAR 5+");

  status = 0;
}

void select_fivedown()
// 5 OD -> 5
{
  displayOnScreen("SHIFT");
  Serial.println("5");

  delay(400);

  analogWrite(linePressurePin, 15);
  analogWrite(shiftPressurePin, 0);
  digitalWrite(gear1And2Plus4And5Pin, LOW);
  digitalWrite(turbineLockupPin, 0);

  displayOnScreen("GEAR 5");

  status = 0;
}

void select_twoup()
// 1 -> 2
{
  displayOnScreen("SHIFT");
  Serial.println("2");

  analogWrite(linePressurePin, 80);
  analogWrite(shiftPressurePin, 90);
  digitalWrite(gear1And2Plus4And5Pin, HIGH);
  digitalWrite(turbineLockupPin, 0);

  delay(600);

  analogWrite(linePressurePin, 0);
  analogWrite(shiftPressurePin, 0);
  digitalWrite(gear1And2Plus4And5Pin, LOW);
  digitalWrite(turbineLockupPin, 0);

  displayOnScreen("GEAR 2");

  status = 0;
}

void select_threeup()
// 2 -> 3
{
  displayOnScreen("SHIFT");
  Serial.println("3");

  analogWrite(linePressurePin, 80);
  analogWrite(shiftPressurePin, 80);
  digitalWrite(gear2And3Pin, HIGH);
  digitalWrite(turbineLockupPin, 0);

  delay(600);

  analogWrite(linePressurePin, 0);
  analogWrite(shiftPressurePin, 0);
  digitalWrite(gear2And3Pin, LOW);
  digitalWrite(turbineLockupPin, 0);

  displayOnScreen("GEAR 3");

  status = 0;
}

void select_fourup()
// 3 -> 4
{
  displayOnScreen("SHIFT");
  Serial.println("4");

  analogWrite(linePressurePin, 90);
  analogWrite(shiftPressurePin, 100);
  digitalWrite(gear3And4Pin, HIGH);
  digitalWrite(turbineLockupPin, 0);

  delay(1200);

  analogWrite(linePressurePin, 0);
  analogWrite(shiftPressurePin, 0);
  digitalWrite(gear3And4Pin, LOW);
  digitalWrite(turbineLockupPin, 0);

  displayOnScreen("GEAR 4");

  status = 0;
}

void displayOnScreen(string stringToDisplay)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso28_tr);
  u8g2.drawStr(1, 29, stringToDisplay);
  u8g2.sendBuffer();
}