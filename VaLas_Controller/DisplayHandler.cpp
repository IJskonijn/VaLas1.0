#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include "DisplayHandler.h"
#include "VaLas_Controller.h"

// 128x64 for 0.96" OLED
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);
const char* stringToDisplayBuffer;
VaLas_Controller::GearLeverPosition tempLeverPosition;
int tempAtfTemp;

DisplayHandler::DisplayHandler()
{
  u8g2.begin();

  tempLeverPosition = VaLas_Controller::GearLeverPosition::Unknown;
  tempAtfTemp = 0;
}

void DisplayHandler::DisplayOnScreen(const char* stringToDisplay)
{
  DisplayOnScreen(stringToDisplay, tempLeverPosition, -1);
}

void DisplayHandler::DisplayOnScreen(const char* stringToDisplay, VaLas_Controller::GearLeverPosition currentLeverPosition, int atfTemp)
{
  String atfTempToDisplay = String("-");
  if (strlen(stringToDisplay) > 0)
  {
    stringToDisplayBuffer = stringToDisplay;
  }

  u8g2.clearBuffer();

  // Draw gear      
  u8g2.setFont(u8g2_font_logisoso28_tr);
  u8g2.drawStr(1, 29, stringToDisplayBuffer);

  // Draw ATF temp
  if (currentLeverPosition != VaLas_Controller::GearLeverPosition::Park && currentLeverPosition != VaLas_Controller::GearLeverPosition::Neutral)
  {
    if (atfTemp > -1)
      atfTempToDisplay = String(atfTemp);
    else
      atfTempToDisplay = tempAtfTemp;
  }

  String tempVar = "ATF: " + atfTempToDisplay;// + String(" C");
  u8g2.setFont(u8g2_font_logisoso18_tr);
  u8g2.drawStr(10, 65, tempVar.c_str());
  u8g2.sendBuffer();

  delay(150);
}

const String DisplayHandler::ToString(VaLas_Controller::GearLeverPosition v)
{
  switch (v)
  {
    case VaLas_Controller::GearLeverPosition::Park:    return "Park";
    case VaLas_Controller::GearLeverPosition::Reverse: return "Reverse";
    case VaLas_Controller::GearLeverPosition::Neutral: return "Neutral";
    case VaLas_Controller::GearLeverPosition::Drive:   return "Drive";
    default:                                           return "Unknown";
  }
}