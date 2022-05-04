#include <Arduino.h>
#include <ArduinoJson.h>
#include <BluetoothSerial.h>
#include "FS.h"
#include "SPIFFS.h"
#include "ShiftConfig.h"
#include "VaLas_Controller.h"

BluetoothSerial SerialBT;
String receivedMessage = "";

ShiftConfig::ShiftConfig()
{
  SerialBT.begin("VaLas_722.6_Controller");
  SPIFFS.begin();
}

void ShiftConfig::ReceiveConfigViaBluetooth(VaLas_Controller::ShiftSetting (&shiftSettings)[6], bool& useCanBus)
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
  
  StaticJsonDocument<385> doc;
  DeserializationError error = deserializeJson(doc, "");

  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  createObjectFromJson(shiftSettings, useCanBus, doc);
}

void ShiftConfig::SendConfigViaBluetooth(VaLas_Controller::ShiftSetting (&shiftSettings)[6], bool& useCanBus)
{
  StaticJsonDocument<512> doc = createJsonFromObject(shiftSettings, useCanBus);

  // serialize the array and send the result to Serial Bluetooth
  serializeJson(doc, SerialBT);
}

void ShiftConfig::LoadDefaultConfig(VaLas_Controller::ShiftSetting (&shiftSettings)[6], bool& useCanBus)
{
  bool isLoadedFromFile = loadConfigFromFile(shiftSettings, useCanBus);
  if (isLoadedFromFile)
    return;

  createDefaultConfig(shiftSettings);
  writeConfigToFile(shiftSettings, useCanBus);
}


bool ShiftConfig::loadConfigFromFile(VaLas_Controller::ShiftSetting (&shiftSettings)[6], bool& useCanBus) {
  const char filePath[16] = "/config.json"; 
  File file = SPIFFS.open(filePath, "r");
  if (!file) {
    Serial.println("Failed to open config file");
    return false;
  }

  StaticJsonDocument<385> doc;
  DeserializationError error = deserializeJson(doc, file);

  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }

  createObjectFromJson(shiftSettings, useCanBus, doc);

  file.close();
  return true;
}

bool ShiftConfig::writeConfigToFile(VaLas_Controller::ShiftSetting (&shiftSettings)[6], bool& useCanBus) {  
  const char filePath[16] = "/config.json";  
  File file = SPIFFS.open(filePath, "w");
  if (!file) {    
    return false;
  }
  
  StaticJsonDocument<512> doc = createJsonFromObject(shiftSettings, useCanBus);

  if (serializeJson(doc, file) == 0) {
    file.close();
    return false;
  }  

  file.close();
  return true;
}

StaticJsonDocument<512> ShiftConfig::createJsonFromObject(VaLas_Controller::ShiftSetting (&shiftSettings)[6], bool& useCanBus)
{
  StaticJsonDocument<512> doc;
  doc["UseCanBus"] = useCanBus;
  JsonArray GearShiftSettings = doc.createNestedArray("GearShiftSettings");

  // add some values
  for(VaLas_Controller::ShiftSetting setting : shiftSettings)
  {
    JsonObject shiftSetting = GearShiftSettings.createNestedObject();
    shiftSetting["Name"] = setting.Name;
    shiftSetting["UpshiftDelay"] = setting.UpshiftDelay;
    shiftSetting["UpshiftLinePressure"] = setting.UpshiftLinePressure;
    shiftSetting["UpshiftShiftPressure"] = setting.UpshiftShiftPressure;
    shiftSetting["UpshiftTorqueConverterLockup"] = setting.UpshiftTorqueConverterLockup;
    shiftSetting["DownshiftDelay"] = setting.DownshiftDelay;
    shiftSetting["DownshiftLinePressure"] = setting.DownshiftLinePressure;
    shiftSetting["DownshiftShiftPressure"] = setting.DownshiftShiftPressure;
    shiftSetting["DownshiftTorqueConverterLockup"] = setting.DownshiftTorqueConverterLockup;
  }

  return doc;
}

void ShiftConfig::createObjectFromJson(VaLas_Controller::ShiftSetting (&shiftSettings)[6], bool& useCanBus, StaticJsonDocument<385> doc)
{
  // extract the values
  useCanBus = doc["UseCanBus"].as<bool>();
  for (int i = 0; i < 6; i++)
  {
    shiftSettings[i].Name = doc["GearShiftSettings"][i]["Name"].as<String>();
    shiftSettings[i].UpshiftDelay = doc["GearShiftSettings"][i]["UpshiftDelay"].as<int>();
    shiftSettings[i].UpshiftLinePressure = doc["GearShiftSettings"][i]["UpshiftLinePressure"].as<int>();
    shiftSettings[i].UpshiftShiftPressure = doc["GearShiftSettings"][i]["UpshiftShiftPressure"].as<int>();
    shiftSettings[i].UpshiftTorqueConverterLockup = doc["GearShiftSettings"][i]["UpshiftTorqueConverterLockup"].as<int>();
    shiftSettings[i].DownshiftDelay = doc["GearShiftSettings"][i]["DownshiftDelay"].as<int>();
    shiftSettings[i].DownshiftLinePressure = doc["GearShiftSettings"][i]["DownshiftLinePressure"].as<int>();
    shiftSettings[i].DownshiftShiftPressure = doc["GearShiftSettings"][i]["DownshiftShiftPressure"].as<int>();
    shiftSettings[i].DownshiftTorqueConverterLockup = doc["GearShiftSettings"][i]["DownshiftTorqueConverterLockup"].as<int>();
  }
}

void ShiftConfig::createDefaultConfig(VaLas_Controller::ShiftSetting (&shiftSettings)[6])
{
  // Gear 1
  // Upshift = 1 > 2
  // Downshift = Not available
  shiftSettings[0].Name = "D1";
  shiftSettings[0].UpshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 80;
  shiftSettings[0].UpshiftShiftPressure = 90;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  shiftSettings[0].DownshiftDelay = 600;
  shiftSettings[0].DownshiftLinePressure = 0;
  shiftSettings[0].DownshiftShiftPressure = 0;
  shiftSettings[0].DownshiftTorqueConverterLockup = 0;
  
  // Gear 2
  // Upshift = 2 > 3
  // Downshift = 2 > 1
  shiftSettings[0].Name = "D2";
  shiftSettings[0].UpshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 80;
  shiftSettings[0].UpshiftShiftPressure = 80;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  shiftSettings[0].DownshiftDelay = 700;
  shiftSettings[0].DownshiftLinePressure = 40;
  shiftSettings[0].DownshiftShiftPressure = 40;
  shiftSettings[0].DownshiftTorqueConverterLockup = 0;
  
  // Gear 3
  // Upshift = 3 > 4
  // Downshift = 3 > 2
  shiftSettings[0].Name = "D3";
  shiftSettings[0].UpshiftDelay = 1200;
  shiftSettings[0].UpshiftLinePressure = 90;
  shiftSettings[0].UpshiftShiftPressure = 100;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  shiftSettings[0].DownshiftDelay = 600;
  shiftSettings[0].DownshiftLinePressure = 180;
  shiftSettings[0].DownshiftShiftPressure = 180;
  shiftSettings[0].DownshiftTorqueConverterLockup = 0;
  
  // Gear 4
  // Upshift = 4 > 5
  // Downshift = 4 > 3
  shiftSettings[0].Name = "D4";
  shiftSettings[0].UpshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 120;
  shiftSettings[0].UpshiftShiftPressure = 120;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  shiftSettings[0].DownshiftDelay = 600;
  shiftSettings[0].DownshiftLinePressure = 140;
  shiftSettings[0].DownshiftShiftPressure = 140;
  shiftSettings[0].DownshiftTorqueConverterLockup = 0;
  
  // Gear 5
  // Upshift = 5 > 5+
  // Downshift = 5 > 4
  shiftSettings[0].Name = "D5";
  shiftSettings[0].UpshiftDelay = 400;
  shiftSettings[0].UpshiftLinePressure = 25;
  shiftSettings[0].UpshiftShiftPressure = 0;
  shiftSettings[0].UpshiftTorqueConverterLockup = 255;
  shiftSettings[0].DownshiftDelay = 600;
  shiftSettings[0].DownshiftLinePressure = 140;
  shiftSettings[0].DownshiftShiftPressure = 140;
  shiftSettings[0].DownshiftTorqueConverterLockup = 0;
  
  // Gear 5+
  // Upshift = Not available
  // Downshift = 5+ > 5
  shiftSettings[0].Name = "D5+";
  shiftSettings[0].UpshiftDelay = 600;
  shiftSettings[0].UpshiftLinePressure = 0;
  shiftSettings[0].UpshiftShiftPressure = 0;
  shiftSettings[0].UpshiftTorqueConverterLockup = 0;
  shiftSettings[0].DownshiftDelay = 400;
  shiftSettings[0].DownshiftLinePressure = 15;
  shiftSettings[0].DownshiftShiftPressure = 0;
  shiftSettings[0].DownshiftTorqueConverterLockup = 0;
}
