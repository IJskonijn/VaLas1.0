#include <Arduino.h>
#include <ArduinoJson.h>
#include "TaskStructs.h"
#include <BluetoothSerial.h>
#include "FS.h"
#include "SPIFFS.h"
#include "ShiftConfig.h"
#include "VaLas_Controller.h"

BluetoothSerial SerialBT;
String receivedMessage = "";
String message = "";
bool spiffsMountingSuccess = false;
bool sentCurrentConfig = false;


#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

ShiftConfig::ShiftConfig()
{
  Serial.println("Init ShiftConfig");
}

void ShiftConfig::init()
{
  if(!SerialBT.begin("VaLas_722.6_Controller"))
  {
    Serial.println("An error occurred initializing Bluetooth");
    return;
  }

  // if (!SPIFFS.begin(true))
  // {
  //   Serial.println("An Error has occurred while mounting SPIFFS");
  //   return;
  // }
  // spiffsMountingSuccess = true;

  vTaskDelay(50);
}

void ShiftConfig::execute(void * parameter)
{
  TaskStructs::ShiftConfigParameters *parameters = (TaskStructs::ShiftConfigParameters*) parameter;
  bool* useCanBusPtr = parameters->useCanBusPtr;
  VaLas_Controller::ShiftSetting* gearboxSettingsPtr = parameters->shiftSettings;
  
  if (SerialBT.available()) // Connected to device
  {
    if(!sentCurrentConfig)
    {
      // send current config here
      sentCurrentConfig = true;
    }

    char incomingChar = SerialBT.read();
    if (incomingChar != '\n'){
      message += String(incomingChar);
    }
    else{
      Serial.println("Full received string: " + String(message));
      test(gearboxSettingsPtr, useCanBusPtr, message);
      message = "";
    }
  }

  vTaskDelay(20);
}

void ShiftConfig::test(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, String message)
{
  message.toLowerCase();
  message.trim();

  // Check received message and control output accordingly
  if (message == "receiveconfigfrommobiledevice"){
    ReceiveConfigViaBluetooth(shiftSettingsPtr, useCanBusPtr);
  }
  else if (message == "sendconfigtomobiledevice"){
    SendConfigViaBluetooth(shiftSettingsPtr, useCanBusPtr);
  }
  else {
    Serial.println("test method failed");
    Serial.println("received message in test method:" + message);
  }
}

String ShiftConfig::generatedJsonWithApp(){
  char json[] = "{\"UseCanBus\":true,\"GearShiftSettings\":[{\"Name\":\"D1\",\"UpshiftDelay\":600,\"UpshiftLinePressure\":0,\"UpshiftShiftPressure\":0,\"UpshiftTorqueConverterLockup\":0,\"DownshiftDelay\":600,\"DownshiftLinePressure\":0,\"DownshiftShiftPressure\":0,\"DownshiftTorqueConverterLockup\":0},{\"Name\":\"D2\",\"UpshiftDelay\":600,\"UpshiftLinePressure\":0,\"UpshiftShiftPressure\":0,\"UpshiftTorqueConverterLockup\":0,\"DownshiftDelay\":600,\"DownshiftLinePressure\":0,\"DownshiftShiftPressure\":0,\"DownshiftTorqueConverterLockup\":0},{\"Name\":\"D3\",\"UpshiftDelay\":600,\"UpshiftLinePressure\":0,\"UpshiftShiftPressure\":0,\"UpshiftTorqueConverterLockup\":0,\"DownshiftDelay\":600,\"DownshiftLinePressure\":0,\"DownshiftShiftPressure\":0,\"DownshiftTorqueConverterLockup\":0},{\"Name\":\"D4\",\"UpshiftDelay\":600,\"UpshiftLinePressure\":0,\"UpshiftShiftPressure\":0,\"UpshiftTorqueConverterLockup\":0,\"DownshiftDelay\":600,\"DownshiftLinePressure\":0,\"DownshiftShiftPressure\":0,\"DownshiftTorqueConverterLockup\":0},{\"Name\":\"D5\",\"UpshiftDelay\":600,\"UpshiftLinePressure\":0,\"UpshiftShiftPressure\":0,\"UpshiftTorqueConverterLockup\":0,\"DownshiftDelay\":600,\"DownshiftLinePressure\":0,\"DownshiftShiftPressure\":0,\"DownshiftTorqueConverterLockup\":0},{\"Name\":\"D5\\u002B\",\"UpshiftDelay\":600,\"UpshiftLinePressure\":0,\"UpshiftShiftPressure\":0,\"UpshiftTorqueConverterLockup\":0,\"DownshiftDelay\":600,\"DownshiftLinePressure\":0,\"DownshiftShiftPressure\":0,\"DownshiftTorqueConverterLockup\":0}]}";
  return String(json);
}

void ShiftConfig::ReceiveConfigViaBluetooth(VaLas_Controller::ShiftSetting* shiftsettingsptr, bool* usecanbusptr)
{
  // if (SerialBT.available()){
  //   char incomingChar = SerialBT.read();
  //   if (incomingChar != '\n'){
  //     receivedMessage += String(incomingChar);
  //   }
  //   else{
  //     receivedMessage = "";
  //   }
  // }
  receivedMessage = generatedJsonWithApp();
  Serial.println(receivedMessage);
  
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, receivedMessage);

  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  createObjectFromJson(shiftsettingsptr, usecanbusptr, doc);
  Serial.println("New useCanBus value after reading JSON and converting: " + String(*usecanbusptr));
}

void ShiftConfig::SendConfigViaBluetooth(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr)
{
  StaticJsonDocument<2048> doc = createJsonFromObject(shiftSettingsPtr, useCanBusPtr);

  // serialize the array and send the result to Serial Bluetooth
  serializeJson(doc, SerialBT);
}

void ShiftConfig::LoadDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr)
{
  if (spiffsMountingSuccess)
  {
    bool isLoadedFromFile = loadConfigFromFile(shiftSettingsPtr, useCanBusPtr);
    if (isLoadedFromFile)
      return;

    createDefaultConfig(shiftSettingsPtr);
    writeConfigToFile(shiftSettingsPtr, useCanBusPtr);
  }
  else
  {
    createDefaultConfig(shiftSettingsPtr);
  }
}


bool ShiftConfig::loadConfigFromFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr) {
  // const char filePath[16] = "/config.json"; 
  // File file = SPIFFS.open(filePath, "r");
  // if (!file) {
  //   Serial.println("Failed to open config file");
  //   return false;
  // }

  // StaticJsonDocument<2048> doc;
  // DeserializationError error = deserializeJson(doc, file);

  // if (error)
  // {
  //   Serial.print(F("deserializeJson() failed: "));
  //   Serial.println(error.f_str());
  //   return false;
  // }

  // createObjectFromJson(shiftSettings, useCanBus, doc);

  // file.close();
  // return true;
}

bool ShiftConfig::writeConfigToFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr) {  
  const char filePath[16] = "/config.json";  
  File file = SPIFFS.open(filePath, "w");
  if (!file) {    
    return false;
  }
  
  StaticJsonDocument<2048> doc = createJsonFromObject(shiftSettingsPtr, useCanBusPtr);

  if (serializeJson(doc, file) == 0) {
    file.close();
    return false;
  }  

  file.close();
  return true;
}

StaticJsonDocument<2048> ShiftConfig::createJsonFromObject(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr)
{
  StaticJsonDocument<2048> doc;
  doc["UseCanBus"] = *useCanBusPtr;
  JsonArray GearShiftSettings = doc.createNestedArray("GearShiftSettings");

  // add some values
  VaLas_Controller::ShiftSetting (shiftSettings)[6] = *shiftSettingsPtr;
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

void ShiftConfig::createObjectFromJson(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, StaticJsonDocument<2048> doc)
{
  // extract the values
  *useCanBusPtr = doc["UseCanBus"].as<bool>();
  for (int i = 0; i < 6; i++)
  {
    shiftSettingsPtr[i].Name = doc["GearShiftSettings"][i]["Name"].as<String>();
    shiftSettingsPtr[i].UpshiftDelay = doc["GearShiftSettings"][i]["UpshiftDelay"].as<int>();
    shiftSettingsPtr[i].UpshiftLinePressure = doc["GearShiftSettings"][i]["UpshiftLinePressure"].as<int>();
    shiftSettingsPtr[i].UpshiftShiftPressure = doc["GearShiftSettings"][i]["UpshiftShiftPressure"].as<int>();
    shiftSettingsPtr[i].UpshiftTorqueConverterLockup = doc["GearShiftSettings"][i]["UpshiftTorqueConverterLockup"].as<int>();
    shiftSettingsPtr[i].DownshiftDelay = doc["GearShiftSettings"][i]["DownshiftDelay"].as<int>();
    shiftSettingsPtr[i].DownshiftLinePressure = doc["GearShiftSettings"][i]["DownshiftLinePressure"].as<int>();
    shiftSettingsPtr[i].DownshiftShiftPressure = doc["GearShiftSettings"][i]["DownshiftShiftPressure"].as<int>();
    shiftSettingsPtr[i].DownshiftTorqueConverterLockup = doc["GearShiftSettings"][i]["DownshiftTorqueConverterLockup"].as<int>();
  }
}

void ShiftConfig::createDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettings)
{
  Serial.println("Creating a default config");

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
  shiftSettings[1].Name = "D2";
  shiftSettings[1].UpshiftDelay = 600;
  shiftSettings[1].UpshiftLinePressure = 80;
  shiftSettings[1].UpshiftShiftPressure = 80;
  shiftSettings[1].UpshiftTorqueConverterLockup = 0;
  shiftSettings[1].DownshiftDelay = 700;
  shiftSettings[1].DownshiftLinePressure = 40;
  shiftSettings[1].DownshiftShiftPressure = 40;
  shiftSettings[1].DownshiftTorqueConverterLockup = 0;
  
  // Gear 3
  // Upshift = 3 > 4
  // Downshift = 3 > 2
  shiftSettings[2].Name = "D3";
  shiftSettings[2].UpshiftDelay = 1200;
  shiftSettings[2].UpshiftLinePressure = 90;
  shiftSettings[2].UpshiftShiftPressure = 100;
  shiftSettings[2].UpshiftTorqueConverterLockup = 0;
  shiftSettings[2].DownshiftDelay = 600;
  shiftSettings[2].DownshiftLinePressure = 180;
  shiftSettings[2].DownshiftShiftPressure = 180;
  shiftSettings[2].DownshiftTorqueConverterLockup = 0;
  
  // Gear 4
  // Upshift = 4 > 5
  // Downshift = 4 > 3
  shiftSettings[3].Name = "D4";
  shiftSettings[3].UpshiftDelay = 600;
  shiftSettings[3].UpshiftLinePressure = 120;
  shiftSettings[3].UpshiftShiftPressure = 120;
  shiftSettings[3].UpshiftTorqueConverterLockup = 0;
  shiftSettings[3].DownshiftDelay = 600;
  shiftSettings[3].DownshiftLinePressure = 140;
  shiftSettings[3].DownshiftShiftPressure = 140;
  shiftSettings[3].DownshiftTorqueConverterLockup = 0;
  
  // Gear 5
  // Upshift = 5 > 5+
  // Downshift = 5 > 4
  shiftSettings[4].Name = "D5";
  shiftSettings[4].UpshiftDelay = 400;
  shiftSettings[4].UpshiftLinePressure = 25;
  shiftSettings[4].UpshiftShiftPressure = 0;
  shiftSettings[4].UpshiftTorqueConverterLockup = 255;
  shiftSettings[4].DownshiftDelay = 600;
  shiftSettings[4].DownshiftLinePressure = 140;
  shiftSettings[4].DownshiftShiftPressure = 140;
  shiftSettings[4].DownshiftTorqueConverterLockup = 0;
  
  // Gear 5+
  // Upshift = Not available
  // Downshift = 5+ > 5
  shiftSettings[5].Name = "D5+";
  shiftSettings[5].UpshiftDelay = 600;
  shiftSettings[5].UpshiftLinePressure = 0;
  shiftSettings[5].UpshiftShiftPressure = 0;
  shiftSettings[5].UpshiftTorqueConverterLockup = 0;
  shiftSettings[5].DownshiftDelay = 400;
  shiftSettings[5].DownshiftLinePressure = 15;
  shiftSettings[5].DownshiftShiftPressure = 0;
  shiftSettings[5].DownshiftTorqueConverterLockup = 0;
}
