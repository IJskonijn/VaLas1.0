#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "DisplayHandler.h"
#include "VaLas_Controller.h"
#include "TaskStructs.h"

// Forward declaration for runtime display size getter
extern bool getDisplayIsLarge();

// 128x64 for 0.96" OLED
// 128x32 for 0.91" OLED
DisplayHandler::DisplayHandler()
  : largeDisplay(U8G2_R0, /* reset=*/ U8X8_PIN_NONE),
    smallDisplay(U8G2_R0, /* reset=*/ U8X8_PIN_NONE),
    activeDisplay(nullptr) {
}

void DisplayHandler::begin()
{
  if (getDisplayIsLarge()) {
    activeDisplay = &largeDisplay;
    u8g2_x_coordinate = 10;
    u8g2_y_coordinate = 34;
    u8g2_selectedFont = u8g2_font_logisoso34_tr;
  } else {
    activeDisplay = &smallDisplay;
    u8g2_x_coordinate = 1;
    u8g2_y_coordinate = 32;
    u8g2_selectedFont = u8g2_font_logisoso30_tr;
  }

  String is096oled = getDisplayIsLarge() ? "true" : "false";
  Serial.println("Init displayhandler");
  Serial.println("Is using 0.96 OLED: " + is096oled);
  Serial.println("Using display y coordinate: " + String(u8g2_y_coordinate));

  // Use the same pins as the ESP32 default, but initialize them explicitly so
  // the display remains independent of board-core defaults.
  Wire.begin(21, 22);
  Wire.setTimeOut(100);
  
  Wire.setClock(20000);  // Lower clock speed for better stability over longer wires
  activeDisplay->begin();
}

void DisplayHandler::execute(void * parameter)
{
  TaskStructs::DisplayHandlerParameters *parameters = (TaskStructs::DisplayHandlerParameters*) parameter;
  VaLas_Controller::DisplayScreen screenToDisplay = *(parameters->screenToDisplayPtr);
  VaLas_Controller::GearLeverPosition currentLeverPosition = *(parameters->currentLeverPositionPtr);
  int currentGear = *(parameters->currentGearPtr);
  int atfTemp = *(parameters->atfTempPtr);

  activeDisplay->clearBuffer();
  
  switch (screenToDisplay){
    case VaLas_Controller::DisplayScreen::Main:
      displayMainScreen(currentLeverPosition, currentGear, atfTemp);
      activeDisplay->sendBuffer();
      break;
    case VaLas_Controller::DisplayScreen::Shifting:
      displayShifting();
      activeDisplay->sendBuffer();
      vTaskDelay(500);
      break;
  }
}

void DisplayHandler::DisplayStartupOnScreen()
{
  activeDisplay->clearBuffer();
  activeDisplay->setFont(u8g2_font_logisoso30_tr);
  activeDisplay->drawStr(1, u8g2_y_coordinate, "VaLas");
  activeDisplay->sendBuffer();
  
  vTaskDelay(1000); // delay(1500);

  activeDisplay->clearBuffer();
  activeDisplay->setFont(u8g2_font_logisoso30_tr);
  activeDisplay->drawStr(1, u8g2_y_coordinate, "Ver. 1.1");
  activeDisplay->sendBuffer();

  vTaskDelay(1000); // delay(1500);
}

void DisplayHandler::displayMainScreen(const VaLas_Controller::GearLeverPosition currentLeverPosition, const int currentGear, const int atfTemp)
{
  Serial.println("Printing gear on screen: " + String(currentGear));
  String atfTempToDisplay = String("-");

  // Draw gear      
  activeDisplay->setFont(u8g2_selectedFont);
  activeDisplay->drawStr(u8g2_x_coordinate, u8g2_y_coordinate, ToString(currentLeverPosition, currentGear).c_str());

  // Draw ATF temp
  if ((currentLeverPosition == VaLas_Controller::GearLeverPosition::Drive || currentLeverPosition == VaLas_Controller::GearLeverPosition::Reverse) && atfTemp > -1)
    atfTempToDisplay = String(atfTemp);

  String tempVar = "ATF: " + atfTempToDisplay;// + String(" C");
  Serial.println(tempVar);

  if (getDisplayIsLarge())
  {
      activeDisplay->setFont(u8g2_font_logisoso18_tr);
      activeDisplay->drawStr(10, 62, tempVar.c_str());
  }
  else
  {
      activeDisplay->setFont(u8g2_font_logisoso16_tr);
      int atfWidth = activeDisplay->getStrWidth(tempVar.c_str());
      activeDisplay->drawStr(128 - atfWidth - 2, 26, tempVar.c_str());  // 2px marge van rechterrand
  }
}

void DisplayHandler::displayShifting()
{
  activeDisplay->setFont(u8g2_selectedFont);
  activeDisplay->drawStr(1, u8g2_y_coordinate, " SHIFT");
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
    case 5:  return "" + screenVar + String(currentGear);
    case 6:  return "D5+";
    default: return "U";
  }
}
