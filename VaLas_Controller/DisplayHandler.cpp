#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "DisplayHandler.h"
#include "VaLas_Controller.h"
#include "TaskStructs.h"

// 128x64 for 0.96" OLED

//VaLas_Controller::DisplayScreen* screenToDisplay;
DisplayHandler::DisplayHandler() : u8g2(U8G2_R0){}

void DisplayHandler::begin(VaLas_Controller::DisplayScreen* screenToDisplayPtr)
{
  Serial.println("Init displayhandler");
  u8g2.begin(); 
  //screenToDisplay = screenToDisplayPtr;
}

void DisplayHandler::execute(void * parameter)
{
  TaskStructs::DisplayHandlerParameters *parameters = (TaskStructs::DisplayHandlerParameters*) parameter;
  VaLas_Controller::DisplayScreen screenToDisplay = *(parameters->screenToDisplayPtr);
  VaLas_Controller::GearLeverPosition currentLeverPosition = *(parameters->currentLeverPositionPtr);
  int currentGear = *(parameters->currentGearPtr);
  int atfTemp = *(parameters->atfTempPtr);

  u8g2.clearBuffer();
  
  Serial.print("Displayhandler execute: currentGear = " + currentGear);
  Serial.println(" and atfTemp = " + atfTemp);
  switch (screenToDisplay){
    case VaLas_Controller::DisplayScreen::Main:
      displayMainScreen(currentLeverPosition, currentGear, atfTemp);
      u8g2.sendBuffer();
      break;
    case VaLas_Controller::DisplayScreen::Shifting:
      displayShifting();
      u8g2.sendBuffer();
      vTaskDelay(500);
      break;
  }
}

void DisplayHandler::DisplayStartupOnScreen()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso28_tr);
  u8g2.drawStr(1, 29, "VaLas");
  u8g2.sendBuffer();
  
  vTaskDelay(1500); // delay(1500);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso28_tr);
  u8g2.drawStr(1, 29, "Ver. 1.1");
  u8g2.sendBuffer();

  vTaskDelay(1500); // delay(1500);
}

void DisplayHandler::displayMainScreen(const VaLas_Controller::GearLeverPosition currentLeverPosition, const int currentGear, const int atfTemp)
{
  Serial.println("Printing gear on screen: " + currentGear);
  String atfTempToDisplay = String("-");

  // Draw gear      
  u8g2.setFont(u8g2_font_logisoso28_tr);
  u8g2.drawStr(1, 29, ToString(currentLeverPosition, currentGear).c_str());

  // Draw ATF temp
  if (currentLeverPosition != VaLas_Controller::GearLeverPosition::Unknown)
  {
    if (currentLeverPosition != VaLas_Controller::GearLeverPosition::Park && currentLeverPosition != VaLas_Controller::GearLeverPosition::Neutral)
    {
      if (atfTemp > -1)
        atfTempToDisplay = String(atfTemp);
    }

    String tempVar = "ATF: " + atfTempToDisplay;// + String(" C");
    u8g2.setFont(u8g2_font_logisoso18_tr);
    u8g2.drawStr(10, 65, tempVar.c_str());
  }
}

void DisplayHandler::displayShifting()
{
  u8g2.setFont(u8g2_font_logisoso28_tr);
  u8g2.drawStr(1, 29, " SHIFT");
}

const String DisplayHandler::ToString(const VaLas_Controller::GearLeverPosition leverPosition)
{
  switch (leverPosition)
  {
    case VaLas_Controller::GearLeverPosition::Park:    return "Park";
    case VaLas_Controller::GearLeverPosition::Reverse: return "Reverse";
    case VaLas_Controller::GearLeverPosition::Neutral: return "Neutral";
    case VaLas_Controller::GearLeverPosition::Drive:   return "Drive";
    default:                                           return "Unknown";
  }
}

const String DisplayHandler::ToString(const VaLas_Controller::GearLeverPosition leverPosition, const int currentGear)
{
  String printVar = ToString(leverPosition);
  String screenVar = "" + printVar.substring(0,1); // Take first character. Example Park would print: P

  // For now display the current gear in every lever position except P. (For dev purposes)
  // If not needed anymore, change implementation to only display gear number in D or D and R.
  if (screenVar == "P")
    return screenVar;

  switch (currentGear)
  {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:  return "" + screenVar + currentGear;
    case 6:  return "D5+";
    default: return "U";
  }
}