#include <Arduino.h>
#include <ArduinoJson.h>
#include "TaskStructs.h"
#include "FS.h"
#include "SPIFFS.h"
#include "ShiftConfig.h"
#include "VaLas_Controller.h"
#include <WiFi.h>
#include <ESP32WebServer.h>

bool spiffsMountingSuccess = false;

// ----- WiFi / Web server configuration -----
const char* kApSsid = "VaLas_722.6_Controller";
const char* kApPassword = "12345678";

ESP32WebServer webServer(80);
bool webServerInitialized = false;

// Pointers to the live configuration used by the rest of the system.
static VaLas_Controller::ShiftSetting* g_shiftSettingsPtr = nullptr;
static bool* g_useCanBusPtr = nullptr;
static bool* g_usePedalShiftersPtr = nullptr;

static void handleRoot();
static void handleSave();

ShiftConfig::ShiftConfig()
{
  Serial.println("Init ShiftConfig");
}

void ShiftConfig::init()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    spiffsMountingSuccess = false;
  }
  else
  {
    spiffsMountingSuccess = true;
  }

  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(kApSsid, kApPassword))
  {
    Serial.print("WiFi AP started. Connect to SSID: ");
    Serial.println(kApSsid);
    Serial.println("IP address: " + WiFi.softAPIP().toString());
  }
  else
  {
    Serial.println("Failed to start WiFi AP");
  }

  vTaskDelay(50);
}

void ShiftConfig::execute(void * parameter)
{
  TaskStructs::ShiftConfigParameters *parameters = (TaskStructs::ShiftConfigParameters*) parameter;
  bool* useCanBusPtr = parameters->useCanBusPtr;
  bool* usePedalShiftersPtr = parameters->usePedalShiftersPtr;
  VaLas_Controller::ShiftSetting* gearboxSettingsPtr = parameters->shiftSettings;

  if (!webServerInitialized)
  {
    g_shiftSettingsPtr = gearboxSettingsPtr;
    g_useCanBusPtr = useCanBusPtr;
    g_usePedalShiftersPtr = usePedalShiftersPtr;

    webServer.on("/", HTTP_GET, handleRoot);
    webServer.on("/save", HTTP_POST, handleSave);
    webServer.begin();
    webServerInitialized = true;

    Serial.println("ShiftConfig web server started. Open http://" + WiFi.softAPIP().toString() + "/");
  }

  webServer.handleClient();

  vTaskDelay(20);
}

void ShiftConfig::LoadDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr)
{
  if (spiffsMountingSuccess)
  {
    bool isLoadedFromFile = loadConfigFromFile(shiftSettingsPtr, useCanBusPtr, usePedalShiftersPtr);
    if (isLoadedFromFile)
      return;

    createDefaultConfig(shiftSettingsPtr);
    writeConfigToFile(shiftSettingsPtr, useCanBusPtr, usePedalShiftersPtr);
  }
  else
  {
    createDefaultConfig(shiftSettingsPtr);
  }
}

bool ShiftConfig::loadConfigFromFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr) {
  const char filePath[16] = "/config.json"; 
  File file = SPIFFS.open(filePath, "r");
  if (!file) {
    Serial.println("Failed to open config file");
    return false;
  }

  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, file);

  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }

  createObjectFromJson(shiftSettingsPtr, useCanBusPtr, usePedalShiftersPtr, doc);

  file.close();
  return true;
}

bool ShiftConfig::writeConfigToFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr) {  
  const char filePath[16] = "/config.json";  
  File file = SPIFFS.open(filePath, "w");
  if (!file) {    
    return false;
  }
  
  StaticJsonDocument<2048> doc = createJsonFromObject(shiftSettingsPtr, useCanBusPtr, usePedalShiftersPtr);

  if (serializeJson(doc, file) == 0) {
    file.close();
    return false;
  }  

  file.close();
  return true;
}

StaticJsonDocument<2048> ShiftConfig::createJsonFromObject(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr)
{
  StaticJsonDocument<2048> doc;
  doc["UseCanBus"] = *useCanBusPtr;
  doc["UsePedalShifters"] = *usePedalShiftersPtr;
  JsonArray GearShiftSettings = doc.createNestedArray("GearShiftSettings");

  // add some values
  for (int i = 0; i < 6; i++)
  {
    VaLas_Controller::ShiftSetting setting = shiftSettingsPtr[i];
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

void ShiftConfig::createObjectFromJson(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, StaticJsonDocument<2048> doc)
{
  // extract the values
  *useCanBusPtr = doc["UseCanBus"].as<bool>();
  *usePedalShiftersPtr = doc["UsePedalShifters"].as<bool>();
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
  shiftSettings[3].UpshiftLinePressure = 100; // was 120, now 100 to match original code
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

void ShiftConfig::SaveConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr)
{
  if (!spiffsMountingSuccess)
  {
    Serial.println("SPIFFS not mounted; cannot save config");
    return;
  }

  if (!writeConfigToFile(shiftSettingsPtr, useCanBusPtr, usePedalShiftersPtr))
  {
    Serial.println("Failed to save config to file");
  }
}


// ----- HTTP handlers -----

// Ideas: Show default values behind textbox
//        Reset button with confirmation dialog
//        Auto refresh after save / reset (Now it shows 404)
//        Textbox width can be 1/4 or 1/5 length
//        Log all (web) actions to console, like save and reset
//        Log wifi password on first boot?
//        Setting for display size like UseCanBus

static void handleRoot()
{
  if (!g_shiftSettingsPtr || !g_useCanBusPtr || !g_usePedalShiftersPtr)
  {
    webServer.send(500, "text/plain", "Configuration not initialised yet");
    return;
  }

  String html;
  html.reserve(4096);
  html += F("<html><head><meta charset='utf-8'><title>VaLas Shift Config</title></head><body>");
  html += F("<h2>Shift Config Editor</h2>");
  html += F("<form method='POST' action='/save'>");

  // Global flags
  html += F("<label><input type='checkbox' name='useCanBus'");
  if (*g_useCanBusPtr) html += F(" checked");
  html += F("> Use CAN bus</label><br>");

  html += F("<label><input type='checkbox' name='usePedalShifters'");
  if (*g_usePedalShiftersPtr) html += F(" checked");
  html += F("> Use pedal shifters</label><br><hr>");

  for (int i = 0; i < 6; i++)
  {
    const VaLas_Controller::ShiftSetting& s = g_shiftSettingsPtr[i];
    html += "<h3>";
    html += s.Name;
    html += "</h3>";

    html += "UpshiftDelay: <input name='u" + String(i) + "d' value='" + String(s.UpshiftDelay) + "'><br>";
    html += "UpshiftLinePressure: <input name='u" + String(i) + "lp' value='" + String(s.UpshiftLinePressure) + "'><br>";
    html += "UpshiftShiftPressure: <input name='u" + String(i) + "sp' value='" + String(s.UpshiftShiftPressure) + "'><br>";
    html += "UpshiftTorqueConverterLockup: <input name='u" + String(i) + "tc' value='" + String(s.UpshiftTorqueConverterLockup) + "'><br>";

    html += "DownshiftDelay: <input name='d" + String(i) + "d' value='" + String(s.DownshiftDelay) + "'><br>";
    html += "DownshiftLinePressure: <input name='d" + String(i) + "lp' value='" + String(s.DownshiftLinePressure) + "'><br>";
    html += "DownshiftShiftPressure: <input name='d" + String(i) + "sp' value='" + String(s.DownshiftShiftPressure) + "'><br>";
    html += "DownshiftTorqueConverterLockup: <input name='d" + String(i) + "tc' value='" + String(s.DownshiftTorqueConverterLockup) + "'><br><hr>";
  }

  html += F("<input type='submit' value='Save'>");
  html += F("</form></body></html>");

  webServer.send(200, "text/html", html);
}

extern ShiftConfig shiftConfig; // defined in VaLas_Controller.ino

static void handleSave()
{
  if (!g_shiftSettingsPtr || !g_useCanBusPtr || !g_usePedalShiftersPtr)
  {
    webServer.send(500, "text/plain", "Configuration not initialised yet");
    return;
  }

  // Checkboxes: only present if checked
  *g_useCanBusPtr = webServer.hasArg("useCanBus");
  *g_usePedalShiftersPtr = webServer.hasArg("usePedalShifters");

  for (int i = 0; i < 6; i++)
  {
    String baseU = "u" + String(i);
    String baseD = "d" + String(i);

    VaLas_Controller::ShiftSetting& s = g_shiftSettingsPtr[i];

    if (webServer.hasArg(baseU + "d"))  s.UpshiftDelay = webServer.arg(baseU + "d").toInt();
    if (webServer.hasArg(baseU + "lp")) s.UpshiftLinePressure = webServer.arg(baseU + "lp").toInt();
    if (webServer.hasArg(baseU + "sp")) s.UpshiftShiftPressure = webServer.arg(baseU + "sp").toInt();
    if (webServer.hasArg(baseU + "tc")) s.UpshiftTorqueConverterLockup = webServer.arg(baseU + "tc").toInt();

    if (webServer.hasArg(baseD + "d"))  s.DownshiftDelay = webServer.arg(baseD + "d").toInt();
    if (webServer.hasArg(baseD + "lp")) s.DownshiftLinePressure = webServer.arg(baseD + "lp").toInt();
    if (webServer.hasArg(baseD + "sp")) s.DownshiftShiftPressure = webServer.arg(baseD + "sp").toInt();
    if (webServer.hasArg(baseD + "tc")) s.DownshiftTorqueConverterLockup = webServer.arg(baseD + "tc").toInt();
  }

  // Persist to SPIFFS via ShiftConfig wrapper
  shiftConfig.SaveConfig(g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr);

  webServer.sendHeader("Location", "/");
  webServer.send(303);
}
