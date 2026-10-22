#include <Arduino.h>
#include <ArduinoJson.h>
#include "TaskStructs.h"
#include "FS.h"
#include "SPIFFS.h"
#include "ShiftConfig.h"
#include "VaLas_Controller.h"
#include "Sensors.h"
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
static bool* g_useLargeDisplayPtr = nullptr;
static bool* g_useThrottlePositionPtr = nullptr;
static VaLas_Controller::ThrottleCalibration* g_throttleSettingsPtr = nullptr;
static VaLas_Controller::PressureTimeMapSettings* g_pressureTimeMapPtr = nullptr;
static VaLas_Controller::LockupControlSettings* g_lockupSettingsPtr = nullptr;

// Static default settings for display purposes
static VaLas_Controller::ShiftSetting g_defaultShiftSettings[6];
static bool g_defaultUseLargeDisplay = false;
static bool g_defaultUseThrottlePosition = false;
static bool g_defaultUseCanBus = false;
static bool g_defaultUsePedalShifters = false;
static VaLas_Controller::PressureTimeMapSettings g_defaultPressureTimeMap;
static VaLas_Controller::LockupControlSettings g_defaultLockupSettings;
static File g_importFile;

static void handleRoot();
static void handleSave();
static void handleReset();
static void handleExport();
static void handleImport();
static void handleImportUpload();
static void handleCalibrateClosed();
static void handleCalibrateWideOpen();
static void handleTpsAdc();
static void handleN2N3Debug();
static void handleAtfDebug();
static void initDefaultSettings();
static void sendAndClose(int code, const char* contentType, const String& content);

extern Sensors sensors;

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
    Serial.println("Pass: " + String(kApPassword));
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
  bool* useLargeDisplayPtr = parameters->useLargeDisplayPtr;
  bool* useThrottlePositionPtr = parameters->useThrottlePositionPtr;
  VaLas_Controller::ThrottleCalibration* throttleSettingsPtr = parameters->throttleSettingsPtr;
  VaLas_Controller::ShiftSetting* gearboxSettingsPtr = parameters->shiftSettings;
  VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr = parameters->pressureTimeMapPtr;
  VaLas_Controller::LockupControlSettings* lockupSettingsPtr = parameters->lockupSettingsPtr;

  if (!webServerInitialized)
  {
    g_shiftSettingsPtr = gearboxSettingsPtr;
    g_useCanBusPtr = useCanBusPtr;
    g_usePedalShiftersPtr = usePedalShiftersPtr;
    g_useLargeDisplayPtr = useLargeDisplayPtr;
    g_useThrottlePositionPtr = useThrottlePositionPtr;
    g_throttleSettingsPtr = throttleSettingsPtr;
    g_pressureTimeMapPtr = pressureTimeMapPtr;
    g_lockupSettingsPtr = lockupSettingsPtr;

    webServer.on("/", HTTP_GET, handleRoot);
    webServer.on("/save", HTTP_POST, handleSave);
    webServer.on("/reset", HTTP_POST, handleReset);
    webServer.on("/export", HTTP_GET, handleExport);
    webServer.on("/import", HTTP_POST, handleImport, handleImportUpload);
    webServer.on("/calibrate/closed", HTTP_POST, handleCalibrateClosed);
    webServer.on("/calibrate/wide-open", HTTP_POST, handleCalibrateWideOpen);
    webServer.on("/tps-adc", HTTP_GET, handleTpsAdc);
    webServer.on("/n2n3-debug", HTTP_GET, handleN2N3Debug);
    webServer.on("/atf-debug", HTTP_GET, handleAtfDebug);
    webServer.begin();
    webServerInitialized = true;

    Serial.println("ShiftConfig web server started. Open http://" + WiFi.softAPIP().toString() + "/");
  }

  webServer.handleClient();

  vTaskDelay(20);
}

void ShiftConfig::LoadDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr)
{
  // Expose the config pointers now (e.g. for getDisplayIsLarge()) instead of waiting for the
  // web server task's first tick, which runs after displayHandler.begin() during setup().
  g_shiftSettingsPtr = shiftSettingsPtr;
  g_useCanBusPtr = useCanBusPtr;
  g_usePedalShiftersPtr = usePedalShiftersPtr;
  g_useLargeDisplayPtr = useLargeDisplayPtr;
  g_useThrottlePositionPtr = useThrottlePositionPtr;
  g_throttleSettingsPtr = throttleSettingsPtr;
  g_pressureTimeMapPtr = pressureTimeMapPtr;
  g_lockupSettingsPtr = lockupSettingsPtr;

  if (spiffsMountingSuccess)
  {
    bool isLoadedFromFile = loadConfigFromFile(shiftSettingsPtr, useCanBusPtr, usePedalShiftersPtr, useLargeDisplayPtr, useThrottlePositionPtr, throttleSettingsPtr, pressureTimeMapPtr, lockupSettingsPtr);
    if (isLoadedFromFile)
      return;

    CreateDefaultConfig(shiftSettingsPtr);
    writeConfigToFile(shiftSettingsPtr, useCanBusPtr, usePedalShiftersPtr, useLargeDisplayPtr, useThrottlePositionPtr, throttleSettingsPtr, pressureTimeMapPtr, lockupSettingsPtr);
  }
  else
  {
    CreateDefaultConfig(shiftSettingsPtr);
  }
}

bool ShiftConfig::loadConfigFromFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr)
{
  const char filePath[16] = "/config.json"; 
  File file = SPIFFS.open(filePath, "r");
  if (!file) {
    Serial.println("Failed to open config file");
    return false;
  }

  static StaticJsonDocument<3072> doc;
  DeserializationError error = deserializeJson(doc, file);

  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }

  createObjectFromJson(shiftSettingsPtr, useCanBusPtr, usePedalShiftersPtr, useLargeDisplayPtr, useThrottlePositionPtr, throttleSettingsPtr, pressureTimeMapPtr, lockupSettingsPtr, doc);

  file.close();
  return true;
}

bool ShiftConfig::writeConfigToFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr)
{  
  const char filePath[16] = "/config.json";  
  File file = SPIFFS.open(filePath, "w");
  if (!file) {    
    return false;
  }
  
  static StaticJsonDocument<3072> doc;
  createJsonFromObject(shiftSettingsPtr, useCanBusPtr, usePedalShiftersPtr, useLargeDisplayPtr, useThrottlePositionPtr, throttleSettingsPtr, pressureTimeMapPtr, lockupSettingsPtr, doc);

  if (serializeJson(doc, file) == 0) {
    file.close();
    return false;
  }  

  file.close();
  return true;
}

void ShiftConfig::createJsonFromObject(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr, StaticJsonDocument<3072>& doc)
{
  doc.clear();
  doc["UseCanBus"] = *useCanBusPtr;
  doc["UsePedalShifters"] = *usePedalShiftersPtr;
  doc["UseLargeDisplay"] = *useLargeDisplayPtr;
  doc["UseThrottlePosition"] = *useThrottlePositionPtr;
  doc["TpsClosedAdc"] = throttleSettingsPtr->closedAdc;
  doc["TpsWideOpenAdc"] = throttleSettingsPtr->wideOpenAdc;

  JsonObject lockup = doc.createNestedObject("Lockup");
  lockup["Enabled"] = lockupSettingsPtr->enabled;
  lockup["MinGear"] = lockupSettingsPtr->minGear;
  lockup["MinOutputShaftRpm"] = lockupSettingsPtr->minOutputShaftRpm;
  lockup["MaxThrottleForEngage"] = lockupSettingsPtr->maxThrottleForEngage;
  lockup["DecelRpmPerSecThreshold"] = lockupSettingsPtr->decelRpmPerSecThreshold;
  lockup["EngageDelayMs"] = lockupSettingsPtr->engageDelayMs;
  JsonArray lockupRatios = lockup.createNestedArray("GearRatios");
  for (int i = 0; i < 5; i++)
    lockupRatios.add(lockupSettingsPtr->gearRatios[i]);

  JsonObject ptm = doc.createNestedObject("PressureTimeMap");
  ptm["Enabled"] = pressureTimeMapPtr->enabled;
  ptm["ColdTempC"] = pressureTimeMapPtr->coldTempC;
  ptm["WarmTempC"] = pressureTimeMapPtr->warmTempC;
  ptm["HotTempC"] = pressureTimeMapPtr->hotTempC;
  JsonArray pressureRows = ptm.createNestedArray("Pressure");
  JsonArray delayRows = ptm.createNestedArray("Delay");
  for (int row = 0; row < 3; row++)
  {
    JsonArray pressureRow = pressureRows.createNestedArray();
    JsonArray delayRow = delayRows.createNestedArray();
    for (int col = 0; col < 3; col++)
    {
      pressureRow.add(pressureTimeMapPtr->pressurePercent[row][col]);
      delayRow.add(pressureTimeMapPtr->delayPercent[row][col]);
    }
  }

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

}

void ShiftConfig::createObjectFromJson(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr, const StaticJsonDocument<3072>& doc)
{
  // extract the values
  *useCanBusPtr = doc["UseCanBus"].as<bool>();
  *usePedalShiftersPtr = doc["UsePedalShifters"].as<bool>();
  *useLargeDisplayPtr = doc["UseLargeDisplay"].as<bool>();
  *useThrottlePositionPtr = doc["UseThrottlePosition"].as<bool>();
  throttleSettingsPtr->closedAdc = doc["TpsClosedAdc"] | 0;
  throttleSettingsPtr->wideOpenAdc = doc["TpsWideOpenAdc"] | 4095;

  VaLas_Controller::LockupControlSettings defaultLockup;
  JsonObjectConst lockup = doc["Lockup"];
  lockupSettingsPtr->enabled = lockup["Enabled"] | defaultLockup.enabled;
  lockupSettingsPtr->minGear = constrain((int)(lockup["MinGear"] | defaultLockup.minGear), 2, 5);
  lockupSettingsPtr->minOutputShaftRpm = constrain((int)(lockup["MinOutputShaftRpm"] | defaultLockup.minOutputShaftRpm), 0, 8000);
  lockupSettingsPtr->maxThrottleForEngage = constrain((int)(lockup["MaxThrottleForEngage"] | defaultLockup.maxThrottleForEngage), 0, 100);
  lockupSettingsPtr->decelRpmPerSecThreshold = constrain((int)(lockup["DecelRpmPerSecThreshold"] | defaultLockup.decelRpmPerSecThreshold), 0, 5000);
  lockupSettingsPtr->engageDelayMs = constrain((int)(lockup["EngageDelayMs"] | defaultLockup.engageDelayMs), 0, 10000);
  for (int i = 0; i < 5; i++)
    lockupSettingsPtr->gearRatios[i] = lockup["GearRatios"].isNull() ? defaultLockup.gearRatios[i] : (float)(lockup["GearRatios"][i] | defaultLockup.gearRatios[i]);

  VaLas_Controller::PressureTimeMapSettings defaultPtm;
  JsonObjectConst ptm = doc["PressureTimeMap"];
  pressureTimeMapPtr->enabled = ptm["Enabled"] | defaultPtm.enabled;
  pressureTimeMapPtr->coldTempC = ptm["ColdTempC"] | defaultPtm.coldTempC;
  pressureTimeMapPtr->warmTempC = ptm["WarmTempC"] | defaultPtm.warmTempC;
  pressureTimeMapPtr->hotTempC = ptm["HotTempC"] | defaultPtm.hotTempC;
  for (int row = 0; row < 3; row++)
  {
    for (int col = 0; col < 3; col++)
    {
      pressureTimeMapPtr->pressurePercent[row][col] = ptm["Pressure"].isNull() ? defaultPtm.pressurePercent[row][col] : ptm["Pressure"][row][col] | defaultPtm.pressurePercent[row][col];
      pressureTimeMapPtr->delayPercent[row][col] = ptm["Delay"].isNull() ? defaultPtm.delayPercent[row][col] : ptm["Delay"][row][col] | defaultPtm.delayPercent[row][col];
    }
  }

  // Clamp imported values to sane ranges; an out-of-range (e.g. negative) delay would wrap to a
  // huge unsigned value where it is consumed (vTaskDelay / scaleDelay2D) and hang shifting indefinitely.
  for (int i = 0; i < 6; i++)
  {
    shiftSettingsPtr[i].Name = doc["GearShiftSettings"][i]["Name"].as<String>();
    shiftSettingsPtr[i].UpshiftDelay = constrain(doc["GearShiftSettings"][i]["UpshiftDelay"].as<int>(), 50, 5000);
    shiftSettingsPtr[i].UpshiftLinePressure = constrain(doc["GearShiftSettings"][i]["UpshiftLinePressure"].as<int>(), 0, 255);
    shiftSettingsPtr[i].UpshiftShiftPressure = constrain(doc["GearShiftSettings"][i]["UpshiftShiftPressure"].as<int>(), 0, 255);
    shiftSettingsPtr[i].UpshiftTorqueConverterLockup = constrain(doc["GearShiftSettings"][i]["UpshiftTorqueConverterLockup"].as<int>(), 0, 255);
    shiftSettingsPtr[i].DownshiftDelay = constrain(doc["GearShiftSettings"][i]["DownshiftDelay"].as<int>(), 50, 5000);
    shiftSettingsPtr[i].DownshiftLinePressure = constrain(doc["GearShiftSettings"][i]["DownshiftLinePressure"].as<int>(), 0, 255);
    shiftSettingsPtr[i].DownshiftShiftPressure = constrain(doc["GearShiftSettings"][i]["DownshiftShiftPressure"].as<int>(), 0, 255);
    shiftSettingsPtr[i].DownshiftTorqueConverterLockup = constrain(doc["GearShiftSettings"][i]["DownshiftTorqueConverterLockup"].as<int>(), 0, 255);
  }
}

void ShiftConfig::CreateDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettings)
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
  shiftSettings[2].UpshiftDelay = 900; // Was 1200
  shiftSettings[2].UpshiftLinePressure = 90;
  shiftSettings[2].UpshiftShiftPressure = 100;
  shiftSettings[2].UpshiftTorqueConverterLockup = 150; // Steady-state lockup target once trend-gated lockup engages (see LockupControlSettings)
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
  shiftSettings[3].UpshiftTorqueConverterLockup = 190; // Steady-state lockup target once trend-gated lockup engages (see LockupControlSettings)
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
  shiftSettings[4].UpshiftTorqueConverterLockup = 220; // Steady-state lockup target once trend-gated lockup engages (see LockupControlSettings); gear 5+ itself still uses its own fixed 95%
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

void ShiftConfig::SaveConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr)
{
  if (!spiffsMountingSuccess)
  {
    Serial.println("SPIFFS not mounted; cannot save config");
    return;
  }

  if (!writeConfigToFile(shiftSettingsPtr, useCanBusPtr, usePedalShiftersPtr, useLargeDisplayPtr, useThrottlePositionPtr, throttleSettingsPtr, pressureTimeMapPtr, lockupSettingsPtr))
  {
    Serial.println("Failed to save config to file");
  }
}


// ----- HTTP handlers -----

extern ShiftConfig shiftConfig; // defined in VaLas_Controller.ino

// ESP32WebServer only serves one TCP client at a time and otherwise waits up to
// HTTP_MAX_CLOSE_WAIT (2s) for the browser to close its end before accepting the next
// connection. Mobile browsers are often slow to close keep-alive sockets, which was
// starving the 500ms debug/status polling (mobile clients mostly saw stuck/zero readings).
// Force-closing from the server side right after sending fixes this.
static void sendAndClose(int code, const char* contentType, const String& content)
{
  webServer.send(code, contentType, content);
  webServer.client().stop();
}

static void handleRoot()
{
  if (!g_shiftSettingsPtr || !g_useCanBusPtr || !g_usePedalShiftersPtr || !g_useLargeDisplayPtr || !g_useThrottlePositionPtr || !g_throttleSettingsPtr || !g_pressureTimeMapPtr || !g_lockupSettingsPtr)
  {
    sendAndClose(500, "text/plain", "Configuration not initialised yet");
    return;
  }

  // Ensure default settings are initialized
  static bool defaultsInitialized = false;
  if (!defaultsInitialized) {
    initDefaultSettings();
    defaultsInitialized = true;
  }

  String html;
  html.reserve(4096);
  html += F("<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>VaLas Shift Config</title>");
  html += F("<style>body{font-family:Arial,sans-serif;max-width:450px;margin:24px auto;padding:0 16px;color:#222}fieldset{margin:10px 0;padding:8px 10px}.setting-row{display:grid;grid-template-columns:275px 5rem minmax(110px,1fr);gap:6px;align-items:center;margin:4px 0}.setting-row label{min-width:0}.setting-row input{box-sizing:border-box;width:5rem}.setting-row .hint{font-size:.9em;color:#555}.config-actions{margin-top:14px}@media(max-width:450px){body{margin:10px auto;padding:0 8px;font-size:14px}h2{font-size:1.35em}h3{font-size:1.1em}.setting-row{grid-template-columns:minmax(0,1fr) 5rem}.setting-row .hint{grid-column:1 / -1;margin:-2px 0 2px 2px}}</style></head><body>");
  html += F("<h2>Shift Config Editor</h2>");
  html += F("<form method='POST' action='/save'>");

  // Global flags
  html += F("<label><input type='checkbox' name='useCanBus'");
  if (*g_useCanBusPtr) html += F(" checked");
  html += F("> Use CAN bus</label>");
  html += " (default: ";
  html += g_defaultUseCanBus ? "checked" : "unchecked";
  html += ")<br>";

  html += F("<label><input type='checkbox' name='usePedalShifters'");
  if (*g_usePedalShiftersPtr) html += F(" checked");
  html += F("> Use pedal shifters</label>");
  html += " (default: ";
  html += g_defaultUsePedalShifters ? "checked" : "unchecked";
  html += ")<br>";

  html += F("<label><input type='checkbox' name='useLargeDisplay'");
  if (*g_useLargeDisplayPtr) html += F(" checked");
  html += F("> Use 0.96 OLED</label>");
  html += " (default: ";
  html += g_defaultUseLargeDisplay ? "checked" : "unchecked";
  html += ")<br><hr>";

  html += F("<label><input type='checkbox' id='useThrottlePosition' name='useThrottlePosition' onchange='toggleThrottleSections()'");
  if (*g_useThrottlePositionPtr) html += F(" checked");
  html += F("> Use throttle position sensor</label>");
  html += " (default: ";
  html += g_defaultUseThrottlePosition ? "checked" : "unchecked";
  html += ")<br><br>";

  html += F("<details id='sensorDebug'><summary>Debug: N2/N3 + ATF sensor test</summary>");
  html += F("<p class='hint'>Works the same in V1 or V2 (not tied to the throttle position toggle above).</p>");
  html += F("<b>N2/N3 speed sensors</b>");
  html += F("<p class='hint'>Spin the output shaft by hand (ignition on, engine off) and confirm the pulse counts move.</p>");
  html += F("<div class='setting-row'><label>N2 / N3 pulses:</label><span id='n2n3Pulses'>-</span></div>");
  html += F("<div class='setting-row'><label>N2 / N3 RPM:</label><span id='n2n3Rpms'>-</span></div>");
  html += F("<div class='setting-row'><label>Calculated RPM:</label><span id='n2n3Calc'>-</span></div>");
  html += F("<div class='setting-row'><label>RPM trend:</label><span id='n2n3Trend'>-</span></div>");
  html += F("<p class='hint'>N2/N3 RPM should read ~equal in gears 2/3/4, and N2 near 0 in reverse (R1/R2). Trend should sit near 0 at steady cruise, go positive under acceleration, and swing negative on lift-off/braking.</p>");
  html += F("<b>ATF temp sensor</b>");
  html += F("<div class='setting-row'><label>ATF temp:</label><span id='atfTempValue'>-</span></div>");
  html += F("<p class='hint'>The ATF sensor wire is shared with the P/N park-lock switch, so readings always report invalid while in P/N - that's expected. Check this in D or R only.</p>");
  html += F("</details><br>");

  for (int i = 0; i < 6; i++)
  {
    const VaLas_Controller::ShiftSetting& s = g_shiftSettingsPtr[i];
    const VaLas_Controller::ShiftSetting& d = g_defaultShiftSettings[i];
    html += "<h3>";
    html += s.Name;
    html += "</h3>";

  html += F("<fieldset><legend>Upshift:</legend>");
  html += "<div class='setting-row'><label>Delay:</label><input type='number' min='300' max='1500' name='u" + String(i) + "d' value='" + String(s.UpshiftDelay) + "'><span class='hint'>ms (default: " + String(d.UpshiftDelay) + " ms)</span></div>";
  html += "<div class='setting-row'><label>LinePressure:</label><input type='number' min='0' max='255' name='u" + String(i) + "lp' value='" + String(s.UpshiftLinePressure) + "'><span class='hint'>0-255 (default: " + String(d.UpshiftLinePressure) + ")</span></div>";
  html += "<div class='setting-row'><label>ShiftPressure:</label><input type='number' min='0' max='255' name='u" + String(i) + "sp' value='" + String(s.UpshiftShiftPressure) + "'><span class='hint'>0-255 (default: " + String(d.UpshiftShiftPressure) + ")</span></div>";
  html += "<div class='setting-row'><label>TorqueConverterLockup:</label><input type='number' min='0' max='255' name='u" + String(i) + "tc' value='" + String(s.UpshiftTorqueConverterLockup) + "'><span class='hint'>0-255 (default: " + String(d.UpshiftTorqueConverterLockup) + ")</span></div></fieldset>";

  html += F("<fieldset><legend>Downshift:</legend>");
  html += "<div class='setting-row'><label>Delay:</label><input type='number' min='300' max='1500' name='d" + String(i) + "d' value='" + String(s.DownshiftDelay) + "'><span class='hint'>ms (default: " + String(d.DownshiftDelay) + " ms)</span></div>";
  html += "<div class='setting-row'><label>LinePressure:</label><input type='number' min='0' max='255' name='d" + String(i) + "lp' value='" + String(s.DownshiftLinePressure) + "'><span class='hint'>0-255 (default: " + String(d.DownshiftLinePressure) + ")</span></div>";
  html += "<div class='setting-row'><label>ShiftPressure:</label><input type='number' min='0' max='255' name='d" + String(i) + "sp' value='" + String(s.DownshiftShiftPressure) + "'><span class='hint'>0-255 (default: " + String(d.DownshiftShiftPressure) + ")</span></div>";
  html += "<div class='setting-row'><label>TorqueConverterLockup:</label><input type='number' min='0' max='255' name='d" + String(i) + "tc' value='" + String(s.DownshiftTorqueConverterLockup) + "'><span class='hint'>0-255 (default: " + String(d.DownshiftTorqueConverterLockup) + ")</span></div></fieldset><hr>";
  }

  html += *g_useThrottlePositionPtr ? F("<fieldset id='tpsCalibration'><legend>TPS calibration:</legend>") : F("<fieldset id='tpsCalibration' style='display:none'><legend>TPS calibration:</legend>");
  int currentThrottleAdc = sensors.ReadThrottleAdc();
  html += "<div class='setting-row'><label>Current ADC:</label><span id='currentAdcValue'>" + String(currentThrottleAdc) + "</span><span class='hint'>live, move the pedal</span></div>";
  html += "<div class='setting-row'><label>Closed throttle ADC:</label><input type='number' min='0' max='4095' name='tpsClosedAdc' value='" + String(g_throttleSettingsPtr->closedAdc) + "'><span class='hint'><button type='button' onclick=\"fetch('/calibrate/closed',{method:'POST'}).then(()=>window.location.reload());\">Set current</button></span></div>";
  html += "<div class='setting-row'><label>Wide-open throttle ADC:</label><input type='number' min='0' max='4095' name='tpsWideOpenAdc' value='" + String(g_throttleSettingsPtr->wideOpenAdc) + "'><span class='hint'><button type='button' onclick=\"fetch('/calibrate/wide-open',{method:'POST'}).then(()=>window.location.reload());\">Set current</button></span></div></fieldset><br><br>";

  html += *g_useThrottlePositionPtr ? F("<fieldset id='pressureTimeMap'><legend>Pressure/time map (ShiftControlV2 only, throttle x ATF temp):</legend>") : F("<fieldset id='pressureTimeMap' style='display:none'><legend>Pressure/time map (ShiftControlV2 only, throttle x ATF temp):</legend>");
  html += F("<label><input type='checkbox' name='ptmEnabled'");
  if (g_pressureTimeMapPtr->enabled) html += F(" checked");
  html += F("> Enable pressure/time map</label>");
  html += " (default: ";
  html += g_defaultPressureTimeMap.enabled ? "checked" : "unchecked";
  html += ")<br>";
  html += "<div class='setting-row'><label>Cold ATF temp:</label><input type='number' min='-40' max='150' name='ptmColdTempC' value='" + String(g_pressureTimeMapPtr->coldTempC) + "'><span class='hint'>&deg;C (default: " + String(g_defaultPressureTimeMap.coldTempC) + ")</span></div>";
  html += "<div class='setting-row'><label>Warm ATF temp:</label><input type='number' min='-40' max='150' name='ptmWarmTempC' value='" + String(g_pressureTimeMapPtr->warmTempC) + "'><span class='hint'>&deg;C (default: " + String(g_defaultPressureTimeMap.warmTempC) + ")</span></div>";
  html += "<div class='setting-row'><label>Hot ATF temp:</label><input type='number' min='-40' max='150' name='ptmHotTempC' value='" + String(g_pressureTimeMapPtr->hotTempC) + "'><span class='hint'>&deg;C (default: " + String(g_defaultPressureTimeMap.hotTempC) + ")</span></div>";
  html += F("<p class='hint'>Rows: closed / half / full throttle. Columns: cold / warm / hot ATF temp. Values are percent of the configured pressure/delay.</p>");

  const char* rowLabels[3] = {"Closed", "Half", "Full"};
  const char* colLabels[3] = {"Cold", "Warm", "Hot"};

  html += F("<b>Pressure %</b><table>");
  for (int row = 0; row < 3; row++)
  {
    html += "<tr><td>" + String(rowLabels[row]) + "</td>";
    for (int col = 0; col < 3; col++)
    {
      html += "<td><input type='number' min='0' max='300' style='width:4rem' name='ptmP" + String(row) + String(col) + "' value='" + String(g_pressureTimeMapPtr->pressurePercent[row][col]) + "' title='" + String(colLabels[col]) + "'></td>";
    }
    html += "</tr>";
  }
  html += F("</table><br><b>Delay %</b><table>");
  for (int row = 0; row < 3; row++)
  {
    html += "<tr><td>" + String(rowLabels[row]) + "</td>";
    for (int col = 0; col < 3; col++)
    {
      html += "<td><input type='number' min='0' max='300' style='width:4rem' name='ptmD" + String(row) + String(col) + "' value='" + String(g_pressureTimeMapPtr->delayPercent[row][col]) + "' title='" + String(colLabels[col]) + "'></td>";
    }
    html += "</tr>";
  }
  html += F("</table></fieldset><br>");

  html += *g_useThrottlePositionPtr ? F("<fieldset id='lockupSettings'><legend>Torque converter lockup (trend-based, gears 3-5; gear 5+ always locks up):</legend>") : F("<fieldset id='lockupSettings' style='display:none'><legend>Torque converter lockup (trend-based, gears 3-5; gear 5+ always locks up):</legend>");
  html += F("<label><input type='checkbox' name='lockupEnabled'");
  if (g_lockupSettingsPtr->enabled) html += F(" checked");
  html += F("> Enable trend-based lockup</label>");
  html += " (default: ";
  html += g_defaultLockupSettings.enabled ? "checked" : "unchecked";
  html += ")<br>";
  html += "<div class='setting-row'><label>Lowest gear:</label><input type='number' min='2' max='5' name='lockupMinGear' value='" + String(g_lockupSettingsPtr->minGear) + "'><span class='hint'>2-5 (default: " + String(g_defaultLockupSettings.minGear) + ")</span></div>";
  html += "<div class='setting-row'><label>Min output-shaft RPM:</label><input type='number' min='0' max='8000' name='lockupMinRpm' value='" + String(g_lockupSettingsPtr->minOutputShaftRpm) + "'><span class='hint'>RPM, gear-ratio normalized - NOT a road speed (default: " + String(g_defaultLockupSettings.minOutputShaftRpm) + ")</span></div>";
  html += F("<p class='hint'>This box has no calibrated road-speed sensor. \"Min output-shaft RPM\" is the measured turbine RPM divided by the current gear's ratio below, which is proportional to road speed but not a speed reading itself. Dividing by the ratio means the same number here represents roughly the same road speed whether you're cruising in 3rd or 5th, instead of a raw RPM that means a very different speed in each gear.</p>");
  html += "<div class='setting-row'><label>Max throttle to engage:</label><input type='number' min='0' max='100' name='lockupMaxThrottle' value='" + String(g_lockupSettingsPtr->maxThrottleForEngage) + "'><span class='hint'>% (default: " + String(g_defaultLockupSettings.maxThrottleForEngage) + ")</span></div>";
  html += "<div class='setting-row'><label>Decel threshold:</label><input type='number' min='0' max='5000' name='lockupDecelThreshold' value='" + String(g_lockupSettingsPtr->decelRpmPerSecThreshold) + "'><span class='hint'>RPM/sec (default: " + String(g_defaultLockupSettings.decelRpmPerSecThreshold) + ")</span></div>";
  html += "<div class='setting-row'><label>Engage dwell:</label><input type='number' min='0' max='10000' name='lockupEngageDelayMs' value='" + String(g_lockupSettingsPtr->engageDelayMs) + "'><span class='hint'>ms (default: " + String(g_defaultLockupSettings.engageDelayMs) + ")</span></div>";
  html += F("<p class='hint'>Gear ratios (for the RPM normalization above only - not used for shifting). Confirm these match your exact 722.6 variant.</p>");
  html += F("<div class='setting-row'><label>Transmission case:</label><span style='display:flex;flex-direction:column;gap:2px'>");
  html += F("<label style='width:auto;white-space:normal'><input type='radio' id='gearRatioPresetSmall' name='gearRatioPreset' onclick=\"applyGearRatioPreset('small')\"> Small case (&le;330 Nm)</label>");
  html += F("<label style='width:auto;white-space:normal'><input type='radio' id='gearRatioPresetBig' name='gearRatioPreset' onclick=\"applyGearRatioPreset('big')\"> Big case (580 Nm+)</label>");
  html += F("</span></div>");
  html += F("<label><input type='checkbox' id='gearRatioExpertMode' onchange='toggleGearRatioExpertMode()'> Expert mode: edit gear ratios manually</label><br>");
  html += F("<div id='gearRatioFields' style='display:none'>");
  html += F("<p class='hint'>722.6/5G-Tronic 1st-5th ratios (pre-2004 figures). Small case: 3.9319/2.4079/1.4857/1.0000/0.8305. Big case: 3.5876/2.1862/1.4054/1.0000/0.8314. Edit the fields below if your specific variant differs.</p>");
  const char* gearRatioLabels[5] = {"Gear 1 ratio", "Gear 2 ratio", "Gear 3 ratio", "Gear 4 ratio", "Gear 5 ratio"};
  for (int i = 0; i < 5; i++)
  {
    html += "<div class='setting-row'><label>" + String(gearRatioLabels[i]) + ":</label><input type='number' step='any' min='0.1' max='10' name='lockupRatio" + String(i) + "' value='" + String(g_lockupSettingsPtr->gearRatios[i], 4) + "'><span class='hint'>default: " + String(g_defaultLockupSettings.gearRatios[i], 4) + "</span></div>";
  }
  html += F("</div>");
  html += F("<p class='hint'>Per-gear target duty reuses that gear's Upshift TorqueConverterLockup field above (gears 3-5). Requires N2/N3 speed sensors and ShiftControlV2.</p>");
  html += F("</fieldset><br>");

  html += F("<input type='submit' value='Save'>");
  html += F(" <button type='button' onclick=\"if(confirm('Are you sure you want to reset to defaults?')){fetch('/reset',{method:'POST'}).then(()=>window.location.reload());}\">Reset</button>");
  html += F("</form>");
  html += F("<hr><h3>Import / Export</h3>");
  html += F("<form method='GET' action='/export'><button type='submit'>Download current config.json</button></form>");
  html += F("<form method='POST' action='/import' enctype='multipart/form-data'>");
  html += F("<input type='file' name='config' accept='.json,application/json' required>");
  html += F(" <input type='submit' value='Import JSON'>");
  html += F("</form>");
  html += F("<script>var GEAR_RATIO_PRESETS={small:[3.9319,2.4079,1.4857,1.0000,0.8305],big:[3.5876,2.1862,1.4054,1.0000,0.8314]};\nfunction toggleThrottleSections(){var d=document.getElementById('useThrottlePosition').checked?'':'none';document.getElementById('tpsCalibration').style.display=d;document.getElementById('pressureTimeMap').style.display=d;document.getElementById('lockupSettings').style.display=d;}\nfunction applyGearRatioPreset(which){var ratios=GEAR_RATIO_PRESETS[which];for(var i=0;i<5;i++){document.getElementsByName('lockupRatio'+i)[0].value=ratios[i];}}\nfunction toggleGearRatioExpertMode(){document.getElementById('gearRatioFields').style.display=document.getElementById('gearRatioExpertMode').checked?'':'none';}\nfunction matchesPreset(ratios,current){for(var i=0;i<5;i++){if(Math.abs(ratios[i]-current[i])>0.001)return false;}return true;}\nfunction detectGearRatioPreset(){var current=[];for(var i=0;i<5;i++){current.push(parseFloat(document.getElementsByName('lockupRatio'+i)[0].value));}if(matchesPreset(GEAR_RATIO_PRESETS.small,current)){document.getElementById('gearRatioPresetSmall').checked=true;}else if(matchesPreset(GEAR_RATIO_PRESETS.big,current)){document.getElementById('gearRatioPresetBig').checked=true;}else{document.getElementById('gearRatioExpertMode').checked=true;toggleGearRatioExpertMode();}}\ndetectGearRatioPreset();\nsetInterval(function(){if(document.getElementById('tpsCalibration').style.display==='none')return;fetch('/tps-adc').then(function(r){return r.text();}).then(function(v){document.getElementById('currentAdcValue').textContent=v;}).catch(function(){});},500);\nfunction fetchSensorDebug(){if(!document.getElementById('sensorDebug').open)return;fetch('/n2n3-debug').then(function(r){return r.text();}).then(function(v){var p=v.split(',');document.getElementById('n2n3Pulses').textContent=p[0]+' / '+p[1];document.getElementById('n2n3Rpms').textContent=p[2]+' / '+p[3];document.getElementById('n2n3Calc').textContent=p[4];document.getElementById('n2n3Trend').textContent=p[5];}).catch(function(){});fetch('/atf-debug').then(function(r){return r.text();}).then(function(v){var p=v.split(',');document.getElementById('atfTempValue').textContent=p[1]==='1'?(p[0]+' C'):'invalid (P/N?)';}).catch(function(){});}\ndocument.getElementById('sensorDebug').addEventListener('toggle',fetchSensorDebug);\nsetInterval(fetchSensorDebug,500);\nif (window.history.replaceState) { window.history.replaceState(null, null, window.location.href); }</script>");
  html += F("</body></html>");

  sendAndClose(200, "text/html", html);
}

// Handle reset: restore defaults, save, and redirect
static void handleReset()
{
  if (!g_shiftSettingsPtr || !g_useCanBusPtr || !g_usePedalShiftersPtr || !g_useLargeDisplayPtr || !g_useThrottlePositionPtr || !g_throttleSettingsPtr || !g_pressureTimeMapPtr || !g_lockupSettingsPtr)
  {
    sendAndClose(500, "text/plain", "Configuration not initialised yet");
    return;
  }
  
  // Restore defaults
  ShiftConfig::CreateDefaultConfig(g_shiftSettingsPtr);
  *g_useCanBusPtr = g_defaultUseCanBus;
  *g_usePedalShiftersPtr = g_defaultUsePedalShifters;
  *g_useLargeDisplayPtr = g_defaultUseLargeDisplay;
  *g_useThrottlePositionPtr = g_defaultUseThrottlePosition;
  *g_throttleSettingsPtr = VaLas_Controller::ThrottleCalibration();
  *g_pressureTimeMapPtr = VaLas_Controller::PressureTimeMapSettings();
  *g_lockupSettingsPtr = VaLas_Controller::LockupControlSettings();

  // Save to SPIFFS
  shiftConfig.SaveConfig(g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr, g_useLargeDisplayPtr, g_useThrottlePositionPtr, g_throttleSettingsPtr, g_pressureTimeMapPtr, g_lockupSettingsPtr);

  Serial.println("[WEB] Reset action received");
  Serial.print("[WEB] useCanBus: "); Serial.println(*g_useCanBusPtr);
  Serial.print("[WEB] usePedalShifters: "); Serial.println(*g_usePedalShiftersPtr);
  Serial.print("[WEB] useLargeDisplay: "); Serial.println(*g_useLargeDisplayPtr);
  for (int i = 0; i < 6; i++) {
    const VaLas_Controller::ShiftSetting& s = g_shiftSettingsPtr[i];
    Serial.print("[WEB] "); Serial.print(s.Name); Serial.print(": UDelay="); Serial.print(s.UpshiftDelay);
    Serial.print(", ULP="); Serial.print(s.UpshiftLinePressure);
    Serial.print(", USP="); Serial.print(s.UpshiftShiftPressure);
    Serial.print(", UTC="); Serial.print(s.UpshiftTorqueConverterLockup);
    Serial.print(", DDelay="); Serial.print(s.DownshiftDelay);
    Serial.print(", DLP="); Serial.print(s.DownshiftLinePressure);
    Serial.print(", DSP="); Serial.print(s.DownshiftShiftPressure);
    Serial.print(", DTC="); Serial.println(s.DownshiftTorqueConverterLockup);
  }

  webServer.sendHeader("Connection", "close");
  webServer.sendHeader("Location", "/");
  sendAndClose(302, "text/html", "<!DOCTYPE html><html><body>Redirecting...</body></html>");
}

static void handleSave()
{
  if (!g_shiftSettingsPtr || !g_useCanBusPtr || !g_usePedalShiftersPtr || !g_useLargeDisplayPtr || !g_useThrottlePositionPtr || !g_throttleSettingsPtr || !g_pressureTimeMapPtr || !g_lockupSettingsPtr)
  {
    sendAndClose(500, "text/plain", "Configuration not initialised yet");
    return;
  }

  // Checkboxes: only present if checked
  *g_useCanBusPtr = webServer.hasArg("useCanBus");
  *g_usePedalShiftersPtr = webServer.hasArg("usePedalShifters");
  *g_useLargeDisplayPtr = webServer.hasArg("useLargeDisplay");
  *g_useThrottlePositionPtr = webServer.hasArg("useThrottlePosition");
  if (webServer.hasArg("tpsClosedAdc")) g_throttleSettingsPtr->closedAdc = constrain(webServer.arg("tpsClosedAdc").toInt(), 0, 4095);
  if (webServer.hasArg("tpsWideOpenAdc")) g_throttleSettingsPtr->wideOpenAdc = constrain(webServer.arg("tpsWideOpenAdc").toInt(), 0, 4095);
  g_lockupSettingsPtr->enabled = webServer.hasArg("lockupEnabled");
  if (webServer.hasArg("lockupMinGear")) g_lockupSettingsPtr->minGear = constrain(webServer.arg("lockupMinGear").toInt(), 2, 5);
  if (webServer.hasArg("lockupMinRpm")) g_lockupSettingsPtr->minOutputShaftRpm = constrain(webServer.arg("lockupMinRpm").toInt(), 0, 8000);
  if (webServer.hasArg("lockupMaxThrottle")) g_lockupSettingsPtr->maxThrottleForEngage = constrain(webServer.arg("lockupMaxThrottle").toInt(), 0, 100);
  if (webServer.hasArg("lockupDecelThreshold")) g_lockupSettingsPtr->decelRpmPerSecThreshold = constrain(webServer.arg("lockupDecelThreshold").toInt(), 0, 5000);
  if (webServer.hasArg("lockupEngageDelayMs")) g_lockupSettingsPtr->engageDelayMs = constrain(webServer.arg("lockupEngageDelayMs").toInt(), 0, 10000);
  for (int i = 0; i < 5; i++)
  {
    String ratioKey = "lockupRatio" + String(i);
    if (webServer.hasArg(ratioKey)) g_lockupSettingsPtr->gearRatios[i] = constrain(webServer.arg(ratioKey).toFloat(), 0.1f, 10.0f);
  }
  g_pressureTimeMapPtr->enabled = webServer.hasArg("ptmEnabled");
  if (webServer.hasArg("ptmColdTempC")) g_pressureTimeMapPtr->coldTempC = webServer.arg("ptmColdTempC").toInt();
  if (webServer.hasArg("ptmWarmTempC")) g_pressureTimeMapPtr->warmTempC = webServer.arg("ptmWarmTempC").toInt();
  if (webServer.hasArg("ptmHotTempC")) g_pressureTimeMapPtr->hotTempC = webServer.arg("ptmHotTempC").toInt();
  for (int row = 0; row < 3; row++)
  {
    for (int col = 0; col < 3; col++)
    {
      String pKey = "ptmP" + String(row) + String(col);
      String dKey = "ptmD" + String(row) + String(col);
      if (webServer.hasArg(pKey)) g_pressureTimeMapPtr->pressurePercent[row][col] = webServer.arg(pKey).toInt();
      if (webServer.hasArg(dKey)) g_pressureTimeMapPtr->delayPercent[row][col] = webServer.arg(dKey).toInt();
    }
  }
  for (int i = 0; i < 6; i++)
  {
    String baseU = "u" + String(i);
    String baseD = "d" + String(i);

    VaLas_Controller::ShiftSetting& s = g_shiftSettingsPtr[i];

    // Clamp to sane ranges: an out-of-range (e.g. negative) delay would wrap to a huge unsigned
    // value where it is consumed (vTaskDelay / scaleDelay2D) and hang shifting indefinitely.
    if (webServer.hasArg(baseU + "d"))  s.UpshiftDelay = constrain(webServer.arg(baseU + "d").toInt(), 50, 5000);
    if (webServer.hasArg(baseU + "lp")) s.UpshiftLinePressure = constrain(webServer.arg(baseU + "lp").toInt(), 0, 255);
    if (webServer.hasArg(baseU + "sp")) s.UpshiftShiftPressure = constrain(webServer.arg(baseU + "sp").toInt(), 0, 255);
    if (webServer.hasArg(baseU + "tc")) s.UpshiftTorqueConverterLockup = constrain(webServer.arg(baseU + "tc").toInt(), 0, 255);

    if (webServer.hasArg(baseD + "d"))  s.DownshiftDelay = constrain(webServer.arg(baseD + "d").toInt(), 50, 5000);
    if (webServer.hasArg(baseD + "lp")) s.DownshiftLinePressure = constrain(webServer.arg(baseD + "lp").toInt(), 0, 255);
    if (webServer.hasArg(baseD + "sp")) s.DownshiftShiftPressure = constrain(webServer.arg(baseD + "sp").toInt(), 0, 255);
    if (webServer.hasArg(baseD + "tc")) s.DownshiftTorqueConverterLockup = constrain(webServer.arg(baseD + "tc").toInt(), 0, 255);
  }

  // Persist to SPIFFS via ShiftConfig wrapper
  shiftConfig.SaveConfig(g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr, g_useLargeDisplayPtr, g_useThrottlePositionPtr, g_throttleSettingsPtr, g_pressureTimeMapPtr, g_lockupSettingsPtr);
  Serial.println("[WEB] Save action received");
  Serial.print("[WEB] useCanBus: "); Serial.println(*g_useCanBusPtr);
  Serial.print("[WEB] usePedalShifters: "); Serial.println(*g_usePedalShiftersPtr);
  Serial.print("[WEB] useLargeDisplay: "); Serial.println(*g_useLargeDisplayPtr);
  for (int i = 0; i < 6; i++) {
    const VaLas_Controller::ShiftSetting& s = g_shiftSettingsPtr[i];
    Serial.print("[WEB] "); Serial.print(s.Name); Serial.print(": UDelay="); Serial.print(s.UpshiftDelay);
    Serial.print(", ULP="); Serial.print(s.UpshiftLinePressure);
    Serial.print(", USP="); Serial.print(s.UpshiftShiftPressure);
    Serial.print(", UTC="); Serial.print(s.UpshiftTorqueConverterLockup);
    Serial.print(", DDelay="); Serial.print(s.DownshiftDelay);
    Serial.print(", DLP="); Serial.print(s.DownshiftLinePressure);
    Serial.print(", DSP="); Serial.print(s.DownshiftShiftPressure);
    Serial.print(", DTC="); Serial.println(s.DownshiftTorqueConverterLockup);
  }

  webServer.sendHeader("Connection", "close");
  webServer.sendHeader("Location", "/");
  sendAndClose(302, "text/html", "<!DOCTYPE html><html><body>Redirecting...</body></html>");
}

static void handleExport()
{
  if (!g_shiftSettingsPtr || !g_useCanBusPtr || !g_usePedalShiftersPtr || !g_useLargeDisplayPtr || !g_useThrottlePositionPtr || !g_throttleSettingsPtr || !g_pressureTimeMapPtr || !g_lockupSettingsPtr)
  {
    sendAndClose(500, "text/plain", "Configuration not initialised yet");
    return;
  }

  static StaticJsonDocument<3072> doc;
  shiftConfig.createJsonFromObject(
    g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr, g_useLargeDisplayPtr, g_useThrottlePositionPtr, g_throttleSettingsPtr, g_pressureTimeMapPtr, g_lockupSettingsPtr, doc);
  String json;
  serializeJsonPretty(doc, json);
  webServer.sendHeader("Content-Disposition", "attachment; filename=config.json");
  sendAndClose(200, "application/json", json);
}

static void handleImportUpload()
{
  HTTPUpload& upload = webServer.upload();

  if (upload.status == UPLOAD_FILE_START)
  {
    if (g_importFile)
      g_importFile.close();
    g_importFile = SPIFFS.open("/import.json", "w");
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (g_importFile)
      g_importFile.write(upload.buf, upload.currentSize);
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (g_importFile)
      g_importFile.close();
  }
}

static void handleImport()
{
  if (!g_shiftSettingsPtr || !g_useCanBusPtr || !g_usePedalShiftersPtr || !g_useLargeDisplayPtr || !g_useThrottlePositionPtr || !g_throttleSettingsPtr || !g_pressureTimeMapPtr || !g_lockupSettingsPtr)
  {
    sendAndClose(500, "text/plain", "Configuration not initialised yet");
    return;
  }

  if (g_importFile)
    g_importFile.close();

  File file = SPIFFS.open("/import.json", "r");
  if (!file)
  {
    sendAndClose(400, "text/plain", "Import failed: no JSON file received");
    return;
  }

  static StaticJsonDocument<3072> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  SPIFFS.remove("/import.json");

  if (error || !doc["GearShiftSettings"].is<JsonArray>() || doc["GearShiftSettings"].size() != 6)
  {
    sendAndClose(400, "text/plain", "Import failed: invalid config.json format");
    return;
  }

  ShiftConfig::createObjectFromJson(g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr, g_useLargeDisplayPtr, g_useThrottlePositionPtr, g_throttleSettingsPtr, g_pressureTimeMapPtr, g_lockupSettingsPtr, doc);
  if (!shiftConfig.writeConfigToFile(g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr, g_useLargeDisplayPtr, g_useThrottlePositionPtr, g_throttleSettingsPtr, g_pressureTimeMapPtr, g_lockupSettingsPtr))
  {
    sendAndClose(500, "text/plain", "Import failed: could not save configuration");
    return;
  }

  webServer.sendHeader("Connection", "close");
  webServer.sendHeader("Location", "/");
  sendAndClose(302, "text/html", "<!DOCTYPE html><html><body>Configuration imported. Please reboot manually.</body></html>");
}

static void handleCalibrateClosed()
{
  if (!g_throttleSettingsPtr) {
    sendAndClose(500, "text/plain", "Configuration not initialised yet");
    return;
  }

  int rawAdc = sensors.ReadThrottleAdc();
  if (rawAdc < 0) {
    sendAndClose(400, "text/plain", "TPS pin is disabled or unavailable");
    return;
  }

  g_throttleSettingsPtr->closedAdc = rawAdc;
  shiftConfig.SaveConfig(g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr, g_useLargeDisplayPtr, g_useThrottlePositionPtr, g_throttleSettingsPtr, g_pressureTimeMapPtr, g_lockupSettingsPtr);
  sendAndClose(200, "text/plain", "Closed throttle calibration saved");
}

static void handleCalibrateWideOpen()
{
  if (!g_throttleSettingsPtr) {
    sendAndClose(500, "text/plain", "Configuration not initialised yet");
    return;
  }

  int rawAdc = sensors.ReadThrottleAdc();
  if (rawAdc < 0) {
    sendAndClose(400, "text/plain", "TPS pin is disabled or unavailable");
    return;
  }

  g_throttleSettingsPtr->wideOpenAdc = rawAdc;
  shiftConfig.SaveConfig(g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr, g_useLargeDisplayPtr, g_useThrottlePositionPtr, g_throttleSettingsPtr, g_pressureTimeMapPtr, g_lockupSettingsPtr);
  sendAndClose(200, "text/plain", "Wide-open throttle calibration saved");
}

static void handleTpsAdc()
{
  sendAndClose(200, "text/plain", String(sensors.ReadThrottleAdc()));
}

static void handleN2N3Debug()
{
  String csv = String(sensors.GetLastN2Pulses()) + "," + String(sensors.GetLastN3Pulses()) + "," +
    String(sensors.GetLastN2Rpm()) + "," + String(sensors.GetLastN3Rpm()) + "," +
    String(sensors.GetLastCalculatedRpm()) + "," + String(sensors.GetRpmTrendPerSec());
  sendAndClose(200, "text/plain", csv);
}

static void handleAtfDebug()
{
  String csv = String(sensors.GetLastAtfTempC()) + "," + String(sensors.IsLastAtfValid() ? 1 : 0);
  sendAndClose(200, "text/plain", csv);
}

// Helper to initialize the static default settings
static void initDefaultSettings() {
  ShiftConfig::CreateDefaultConfig(g_defaultShiftSettings);
  g_defaultUseLargeDisplay = true;
  g_defaultUseThrottlePosition = false;
  g_defaultUseCanBus = true;
  g_defaultUsePedalShifters = true;
  g_defaultPressureTimeMap = VaLas_Controller::PressureTimeMapSettings();
  g_defaultLockupSettings = VaLas_Controller::LockupControlSettings();
}

bool getDisplayIsLarge() { // true = 128x64, false = 128x32 (0.96" vs 0.91")
  return g_useLargeDisplayPtr ? *g_useLargeDisplayPtr : false;
}
