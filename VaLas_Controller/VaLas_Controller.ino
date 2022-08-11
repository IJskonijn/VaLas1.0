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
#include "ShiftControl.h"
#include "DisplayHandler.h"
#include "Sensors.h"
#include "Gearlever.h"
#include "Gearlever_CAN.h"
#include "Gearlever_Modded.h"

int pwmFreq = 1000;
VaLas_Controller::PwmChannels pwmChannels;

Sensors sensors;
DisplayHandler displayHandler;
ShiftControl shiftControl;

void setup()
{
  Serial.begin(9600); // open the serial port at 9600 bps:
  Serial.write("Begin program");
  Serial.write("\n");

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
  ledcAttachPin(mpcPin, pwmChannels.mpcChannel);
  ledcAttachPin(spcPin, pwmChannels.spcChannel);
  ledcAttachPin(tccPin, pwmChannels.tccChannel);
  ledcAttachPin(y4Pin, pwmChannels.y4Channel);

  // ledcSetup(uint8_t channel, uint32_t frequency, uint8_t resolution_bits);
  ledcSetup(pwmChannels.mpcChannel, pwmFreq, 8); // PWM, 8-bit resolution > 0-255
  ledcSetup(pwmChannels.spcChannel, pwmFreq, 8);
  ledcSetup(pwmChannels.tccChannel, pwmFreq, 8);
  ledcSetup(pwmChannels.y4Channel, pwmFreq, 8);

  // Set all the ELR inputs and outputs
  pinMode(elrTogglePin, INPUT_PULLUP);
  pinMode(elrPwmPin, OUTPUT);
  ledcAttachPin(elrPwmPin, elrChannel);
  ledcSetup(elrChannel, elrPwmFreq, 8);

  shiftControl.Init(&displayHandler, &pwmChannels);

  delay(500);
}

void loop()
{
  shiftControl.DoTheStuff();
}
