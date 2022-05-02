#include <Arduino.h>
#include <ArduinoJson.h>
#include <BluetoothSerial.h>
#include "ShiftConfig.h"
#include "VaLas_Controller.h"

BluetoothSerial SerialBT;
String receivedMessage = "";

ShiftConfig::ShiftConfig()
{
  SerialBT.begin("VaLas_722.6_Controller");
}

void ShiftConfig::ReceiveConfigViaBluetooth()
{
if (SerialBT.available()){
    char incomingChar = SerialBT.read();
    if (incomingChar != '\n'){
      receivedMessage += String(incomingChar);
    }
    else{
      receivedMessage = "";
    }
  }
  
  //char json[] = "{\"sensor\":\"gps\",\"time\":1351824120,\"data\":[48.756080,2.302038]}";

  DynamicJsonDocument doc(1024);
  //deserializeJson(doc, json);
  deserializeJson(doc, receivedMessage);

  const char* sensor = doc["sensor"];
  long time          = doc["time"];
  double latitude    = doc["data"][0];
  double longitude   = doc["data"][1];
}

void ShiftConfig::SendConfigViaBluetooth()
{}

void ShiftConfig::CreateDefaultConfig(VaLas_Controller::ShiftSetting (&shiftSettings)[6])
{
  // Gear 1
  shiftSettings[0].Name = "D1";
  shiftSettings[0].UpshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 0;
  shiftSettings[0].UpshiftShiftPressure = 0;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  shiftSettings[0].DownshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 40;
  shiftSettings[0].UpshiftShiftPressure = 40;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  
  // Gear 2
  shiftSettings[0].Name = "D2";
  shiftSettings[0].UpshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 0;
  shiftSettings[0].UpshiftShiftPressure = 0;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  shiftSettings[0].DownshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 40;
  shiftSettings[0].UpshiftShiftPressure = 40;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  
  // Gear 3
  shiftSettings[0].Name = "D3";
  shiftSettings[0].UpshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 0;
  shiftSettings[0].UpshiftShiftPressure = 0;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  shiftSettings[0].DownshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 40;
  shiftSettings[0].UpshiftShiftPressure = 40;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  
  // Gear 4
  shiftSettings[0].Name = "D4";
  shiftSettings[0].UpshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 0;
  shiftSettings[0].UpshiftShiftPressure = 0;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  shiftSettings[0].DownshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 40;
  shiftSettings[0].UpshiftShiftPressure = 40;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  
  // Gear 5
  shiftSettings[0].Name = "D5";
  shiftSettings[0].UpshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 0;
  shiftSettings[0].UpshiftShiftPressure = 0;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  shiftSettings[0].DownshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 40;
  shiftSettings[0].UpshiftShiftPressure = 40;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  
  // Gear 5+
  shiftSettings[0].Name = "D5+";
  shiftSettings[0].UpshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 0;
  shiftSettings[0].UpshiftShiftPressure = 0;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  shiftSettings[0].DownshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 40;
  shiftSettings[0].UpshiftShiftPressure = 40;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
}