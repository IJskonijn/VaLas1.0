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
static bool* g_invertDriverOutputsPtr = nullptr;

// Gear lever override (debug): lets the web UI force a P/R/N/D position and inject manual
// up/down shift requests when CAN bus communication with the physical lever is unreliable.
// enabled/position are persisted to SPIFFS (see createJsonFromObject/createObjectFromJson) so a
// debug session survives a reboot; the manual shift request itself is deliberately NOT persisted
// since it's a momentary one-shot action, not an ongoing state.
static bool g_gearLeverOverrideEnabled = false;
static VaLas_Controller::GearLeverPosition g_overrideLeverPosition = VaLas_Controller::GearLeverPosition::Drive;
static VaLas_Controller::ShiftRequest g_overrideManualShiftRequest = VaLas_Controller::ShiftRequest::NoShift;

// Static default settings for display purposes
static VaLas_Controller::ShiftSetting g_defaultShiftSettings[6];
static bool g_defaultUseLargeDisplay = false;
static bool g_defaultUseThrottlePosition = false;
static bool g_defaultUseCanBus = false;
static bool g_defaultUsePedalShifters = false;
static VaLas_Controller::PressureTimeMapSettings g_defaultPressureTimeMap;
static VaLas_Controller::LockupControlSettings g_defaultLockupSettings;
static bool g_defaultInvertDriverOutputs = true;
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
static void handleOverrideSave();
static void handleOverrideShiftRequest(VaLas_Controller::ShiftRequest request, const char* requestName);
static void handleOverrideUpshift();
static void handleOverrideDownshift();
static bool applyOverridePositionArg(const String& argName);
static const char* overridePositionName(VaLas_Controller::GearLeverPosition pos);
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
  bool* invertDriverOutputsPtr = parameters->invertDriverOutputsPtr;

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
    g_invertDriverOutputsPtr = invertDriverOutputsPtr;

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
    webServer.on("/override", HTTP_POST, handleOverrideSave);
    webServer.on("/override/upshift", HTTP_POST, handleOverrideUpshift);
    webServer.on("/override/downshift", HTTP_POST, handleOverrideDownshift);
    webServer.begin();
    webServerInitialized = true;

    Serial.println("ShiftConfig web server started. Open http://" + WiFi.softAPIP().toString() + "/");
  }

  webServer.handleClient();

  vTaskDelay(20);
}

void ShiftConfig::LoadDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr, bool* invertDriverOutputsPtr)
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
  g_invertDriverOutputsPtr = invertDriverOutputsPtr;

  if (spiffsMountingSuccess)
  {
    bool isLoadedFromFile = loadConfigFromFile(shiftSettingsPtr, useCanBusPtr, usePedalShiftersPtr, useLargeDisplayPtr, useThrottlePositionPtr, throttleSettingsPtr, pressureTimeMapPtr, lockupSettingsPtr, invertDriverOutputsPtr);
    if (isLoadedFromFile)
      return;

    CreateDefaultConfig(shiftSettingsPtr);
    writeConfigToFile(shiftSettingsPtr, useCanBusPtr, usePedalShiftersPtr, useLargeDisplayPtr, useThrottlePositionPtr, throttleSettingsPtr, pressureTimeMapPtr, lockupSettingsPtr, invertDriverOutputsPtr);
  }
  else
  {
    CreateDefaultConfig(shiftSettingsPtr);
  }
}

bool ShiftConfig::loadConfigFromFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr, bool* invertDriverOutputsPtr)
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

  createObjectFromJson(shiftSettingsPtr, useCanBusPtr, usePedalShiftersPtr, useLargeDisplayPtr, useThrottlePositionPtr, throttleSettingsPtr, pressureTimeMapPtr, lockupSettingsPtr, invertDriverOutputsPtr, doc);

  file.close();
  return true;
}

bool ShiftConfig::writeConfigToFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr, bool* invertDriverOutputsPtr)
{  
  const char filePath[16] = "/config.json";  
  File file = SPIFFS.open(filePath, "w");
  if (!file) {    
    return false;
  }
  
  static StaticJsonDocument<3072> doc;
  createJsonFromObject(shiftSettingsPtr, useCanBusPtr, usePedalShiftersPtr, useLargeDisplayPtr, useThrottlePositionPtr, throttleSettingsPtr, pressureTimeMapPtr, lockupSettingsPtr, invertDriverOutputsPtr, doc);

  if (serializeJson(doc, file) == 0) {
    file.close();
    return false;
  }  

  file.close();
  return true;
}

void ShiftConfig::createJsonFromObject(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr, bool* invertDriverOutputsPtr, StaticJsonDocument<3072>& doc)
{
  doc.clear();
  doc["UseCanBus"] = *useCanBusPtr;
  doc["UsePedalShifters"] = *usePedalShiftersPtr;
  doc["UseLargeDisplay"] = *useLargeDisplayPtr;
  doc["UseThrottlePosition"] = *useThrottlePositionPtr;
  doc["TpsClosedAdc"] = throttleSettingsPtr->closedAdc;
  doc["TpsWideOpenAdc"] = throttleSettingsPtr->wideOpenAdc;
  doc["InvertDriverOutputs"] = *invertDriverOutputsPtr;
  doc["GearLeverOverrideEnabled"] = g_gearLeverOverrideEnabled;
  doc["GearLeverOverridePosition"] = overridePositionName(g_overrideLeverPosition);

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

void ShiftConfig::createObjectFromJson(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr, bool* invertDriverOutputsPtr, const StaticJsonDocument<3072>& doc)
{
  // extract the values
  *useCanBusPtr = doc["UseCanBus"].as<bool>();
  *usePedalShiftersPtr = doc["UsePedalShifters"].as<bool>();
  *useLargeDisplayPtr = doc["UseLargeDisplay"].as<bool>();
  *useThrottlePositionPtr = doc["UseThrottlePosition"].as<bool>();
  throttleSettingsPtr->closedAdc = doc["TpsClosedAdc"] | 0;
  throttleSettingsPtr->wideOpenAdc = doc["TpsWideOpenAdc"] | 4095;

  // Missing key (older saved config) defaults to the current inverting PCB.
  *invertDriverOutputsPtr = doc["InvertDriverOutputs"] | true;

  // Gear lever override (debug): persisted so a stationary debug session survives a reboot, but
  // defaults to safely disabled + Drive if missing (older config.json, or never saved before).
  g_gearLeverOverrideEnabled = doc["GearLeverOverrideEnabled"] | false;
  String overridePositionStr = (const char*)(doc["GearLeverOverridePosition"] | "Drive");
  if (overridePositionStr == "Park") g_overrideLeverPosition = VaLas_Controller::GearLeverPosition::Park;
  else if (overridePositionStr == "Reverse") g_overrideLeverPosition = VaLas_Controller::GearLeverPosition::Reverse;
  else if (overridePositionStr == "Neutral") g_overrideLeverPosition = VaLas_Controller::GearLeverPosition::Neutral;
  else g_overrideLeverPosition = VaLas_Controller::GearLeverPosition::Drive;

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
  shiftSettings[2].UpshiftDelay = 1200; // Was 1200, tried 900
  shiftSettings[2].UpshiftLinePressure = 120; // Was 90
  shiftSettings[2].UpshiftShiftPressure = 120; // Was 100
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
  shiftSettings[3].UpshiftLinePressure = 120; // was 120, now 100 to match original code
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

void ShiftConfig::SaveConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr, bool* invertDriverOutputsPtr)
{
  if (!spiffsMountingSuccess)
  {
    Serial.println("SPIFFS not mounted; cannot save config");
    return;
  }

  if (!writeConfigToFile(shiftSettingsPtr, useCanBusPtr, usePedalShiftersPtr, useLargeDisplayPtr, useThrottlePositionPtr, throttleSettingsPtr, pressureTimeMapPtr, lockupSettingsPtr, invertDriverOutputsPtr))
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
//
// Also disables caching for every response: this page/script changes on every firmware
// re-flash, and without this a browser can silently keep serving a stale cached copy of the
// HTML/JS after an update (no visible error - new behavior just doesn't show up at all),
// unless the user remembers to hard-refresh. Not worth the risk for a low-traffic local page.
static void sendAndClose(int code, const char* contentType, const String& content)
{
  webServer.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  webServer.send(code, contentType, content);
  webServer.client().stop();
}

static void handleRoot()
{
  if (!g_shiftSettingsPtr || !g_useCanBusPtr || !g_usePedalShiftersPtr || !g_useLargeDisplayPtr || !g_useThrottlePositionPtr || !g_throttleSettingsPtr || !g_pressureTimeMapPtr || !g_lockupSettingsPtr || !g_invertDriverOutputsPtr)
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

  html += F("<fieldset><legend>Output inversion (PCB hardware revision):</legend>");
  html += F("<p class='hint'>Enable for the current PCB, whose IXDI602PI inverting gate drivers flip the logic sense of the MPC/SPC/TCC PWM channels and the y3/y4/y5 shift solenoid outputs together. Uncheck only if you revert to the older, non-inverting PCB.</p>");
  html += F("<label><input type='checkbox' name='invertDriverOutputs'");
  if (*g_invertDriverOutputsPtr) html += F(" checked");
  html += F("> Invert MPC/SPC/TCC + shift solenoid outputs</label>");
  html += " (default: ";
  html += g_defaultInvertDriverOutputs ? "checked" : "unchecked";
  html += ")<br></fieldset><br>";

  html += F("<details id='sensorDebug'><summary>Debug:</summary>");
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
  html += F("<div class='setting-row'><label>Raw ADC:</label><span id='atfRawAdc'>-</span></div>");
  html += F("<p class='hint'>The ATF sensor wire is shared with the P/N park-lock switch, so readings always report invalid while in P/N - that's expected. Check this in D or R only. Raw ADC should sit roughly 550-1050 in D/R (varies with temperature); pegged near 0 or near 4095 there points to a wiring fault rather than the P/N quirk.</p>");

  html += F("<hr><b>Gear lever override</b>");
  html += F("<p class='hint'>Use when CAN bus communication with the physical lever is unreliable: forces P/R/N/D from here instead of trusting the CAN/sensor reading. Physical up/down shift buttons (steering wheel / pedal shifters) and the manual buttons below both keep working while this is active.</p>");
  html += F("<label><input type='checkbox' id='gearLeverOverrideEnabled' name='gearLeverOverrideEnabled' onchange='toggleGearLeverOverride(this.checked)'");
  if (g_gearLeverOverrideEnabled) html += F(" checked");
  html += F("> Enable gear lever override</label><br>");
  html += "<div id='gearLeverOverrideControls' style='" + String(g_gearLeverOverrideEnabled ? "" : "display:none") + "'>";
  html += F("<div class='setting-row'><label>Lever position:</label><select id='gearLeverOverridePosition' name='gearLeverOverridePosition' onchange=\"setGearLeverOverridePosition(this.value)\">");
  {
    const VaLas_Controller::GearLeverPosition positions[4] = {
      VaLas_Controller::GearLeverPosition::Park, VaLas_Controller::GearLeverPosition::Reverse,
      VaLas_Controller::GearLeverPosition::Neutral, VaLas_Controller::GearLeverPosition::Drive
    };
    const char* labels[4] = {"Park", "Reverse", "Neutral", "Drive"};
    for (int i = 0; i < 4; i++)
    {
      html += "<option value='" + String(labels[i]) + "'";
      if (positions[i] == g_overrideLeverPosition) html += F(" selected");
      html += ">" + String(labels[i]) + "</option>";
    }
  }
  html += F("</select></div>");
  html += F("<div class='setting-row'><label>Manual shift:</label><span><button type='button' onclick=\"sendGearLeverOverrideShift('downshift')\">Downshift</button> <button type='button' onclick=\"sendGearLeverOverrideShift('upshift')\">Upshift</button></span></div>");
  html += F("</div>");

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
  html += F("</form>");
  html += F("<form method='POST' action='/reset' style='display:inline' onsubmit=\"return confirm('Are you sure you want to reset to defaults?');\"><button type='submit'>Reset</button></form>");
  html += F("<hr><h3>Import / Export</h3>");
  html += F("<form method='GET' action='/export'><button type='submit'>Download current config.json</button></form>");
  html += F("<form method='POST' action='/import' enctype='multipart/form-data'>");
  html += F("<input type='file' name='config' accept='.json,application/json' required>");
  html += F(" <input type='submit' value='Import JSON'>");
  html += F("</form>");

  html += F("<script>\n");
  html += F("var GEAR_RATIO_PRESETS={small:[3.9319,2.4079,1.4857,1.0000,0.8305],big:[3.5876,2.1862,1.4054,1.0000,0.8314]};\n");
  html += F("function toggleThrottleSections(){\n");
  html += F("  var d=document.getElementById('useThrottlePosition').checked?'':'none';\n");
  html += F("  document.getElementById('tpsCalibration').style.display=d;\n");
  html += F("  document.getElementById('pressureTimeMap').style.display=d;\n");
  html += F("  document.getElementById('lockupSettings').style.display=d;\n");
  html += F("}\n");
  html += F("function applyGearRatioPreset(which){\n");
  html += F("  var ratios=GEAR_RATIO_PRESETS[which];\n");
  html += F("  for(var i=0;i<5;i++){ document.getElementsByName('lockupRatio'+i)[0].value=ratios[i]; }\n");
  html += F("}\n");
  html += F("function toggleGearRatioExpertMode(){\n");
  html += F("  document.getElementById('gearRatioFields').style.display=document.getElementById('gearRatioExpertMode').checked?'':'none';\n");
  html += F("}\n");
  html += F("function matchesPreset(ratios,current){\n");
  html += F("  for(var i=0;i<5;i++){ if(Math.abs(ratios[i]-current[i])>0.001) return false; }\n");
  html += F("  return true;\n");
  html += F("}\n");
  html += F("function detectGearRatioPreset(){\n");
  html += F("  var current=[];\n");
  html += F("  for(var i=0;i<5;i++){ current.push(parseFloat(document.getElementsByName('lockupRatio'+i)[0].value)); }\n");
  html += F("  if(matchesPreset(GEAR_RATIO_PRESETS.small,current)){\n");
  html += F("    document.getElementById('gearRatioPresetSmall').checked=true;\n");
  html += F("  } else if(matchesPreset(GEAR_RATIO_PRESETS.big,current)){\n");
  html += F("    document.getElementById('gearRatioPresetBig').checked=true;\n");
  html += F("  } else {\n");
  html += F("    document.getElementById('gearRatioExpertMode').checked=true;\n");
  html += F("    toggleGearRatioExpertMode();\n");
  html += F("  }\n");
  html += F("}\n");
  html += F("detectGearRatioPreset();\n");
  html += F("setInterval(function(){\n");
  html += F("  if(document.getElementById('tpsCalibration').style.display==='none') return;\n");
  html += F("  fetch('/tps-adc').then(function(r){ return r.text(); }).then(function(v){\n");
  html += F("    document.getElementById('currentAdcValue').textContent=v;\n");
  html += F("  }).catch(function(){});\n");
  html += F("},500);\n");
  html += F("function fetchSensorDebug(){\n");
  html += F("  if(!document.getElementById('sensorDebug').open) return;\n");
  html += F("  fetch('/n2n3-debug').then(function(r){ return r.text(); }).then(function(v){\n");
  html += F("    var p=v.split(',');\n");
  html += F("    document.getElementById('n2n3Pulses').textContent=p[0]+' / '+p[1];\n");
  html += F("    document.getElementById('n2n3Rpms').textContent=p[2]+' / '+p[3];\n");
  html += F("    document.getElementById('n2n3Calc').textContent=p[4];\n");
  html += F("    document.getElementById('n2n3Trend').textContent=p[5];\n");
  html += F("  }).catch(function(){});\n");
  html += F("  fetch('/atf-debug').then(function(r){ return r.text(); }).then(function(v){\n");
  html += F("    var p=v.split(',');\n");
  html += F("    document.getElementById('atfTempValue').textContent=p[1]==='1'?(p[0]+' C'):'invalid (P/N?)';\n");
  html += F("    document.getElementById('atfRawAdc').textContent=p[2];\n");
  html += F("  }).catch(function(){});\n");
  html += F("}\n");
  html += F("document.getElementById('sensorDebug').addEventListener('toggle',fetchSensorDebug);\n");
  html += F("setInterval(fetchSensorDebug,500);\n");
  html += F("function postGearLeverOverride(enabled,position){\n");
  html += F("  fetch('/override',{\n");
  html += F("    method:'POST',\n");
  html += F("    keepalive:true,\n");
  html += F("    headers:{'Content-Type':'application/x-www-form-urlencoded'},\n");
  html += F("    body:'enabled='+(enabled?'1':'0')+'&position='+position\n");
  html += F("  }).catch(function(e){ alert('Override update failed to reach the device: '+e); });\n");
  html += F("}\n");
  html += F("function toggleGearLeverOverride(enabled){\n");
  html += F("  document.getElementById('gearLeverOverrideControls').style.display=enabled?'':'none';\n");
  html += F("  postGearLeverOverride(enabled,document.getElementById('gearLeverOverridePosition').value);\n");
  html += F("}\n");
  html += F("function setGearLeverOverridePosition(position){\n");
  html += F("  postGearLeverOverride(document.getElementById('gearLeverOverrideEnabled').checked,position);\n");
  html += F("}\n");
  html += F("function sendGearLeverOverrideShift(which){\n");
  html += F("  function attempt(retrying){\n");
  html += F("    try {\n");
  html += F("      fetch('/override/'+which,{method:'POST'}).then(function(r){\n");
  html += F("        if(!r.ok) return r.text().then(function(t){ alert(t); });\n");
  html += F("      }).catch(function(e){\n");
  html += F("        // The ESP32 only serves one TCP client at a time, so this can collide with the\n");
  html += F("        // 500ms debug polling above; retry once after a short delay before giving up.\n");
  html += F("        if(!retrying) return setTimeout(function(){ attempt(true); },300);\n");
  html += F("        alert('Shift request failed to reach the device: '+e);\n");
  html += F("      });\n");
  html += F("    } catch(e) {\n");
  html += F("      // fetch() itself can throw synchronously (e.g. an unsupported option on an\n");
  html += F("      // older browser) - .catch() above only handles promise rejections, not this.\n");
  html += F("      alert('Shift request threw before it could be sent: '+e);\n");
  html += F("    }\n");
  html += F("  }\n");
  html += F("  attempt(false);\n");
  html += F("}\n");
  html += F("if (window.history.replaceState) { window.history.replaceState(null, null, window.location.href); }\n");
  html += F("</script>");

  html += F("</body></html>");

  sendAndClose(200, "text/html", html);
}

// Handle reset: restore defaults, save, and redirect
static void handleReset()
{
  if (!g_shiftSettingsPtr || !g_useCanBusPtr || !g_usePedalShiftersPtr || !g_useLargeDisplayPtr || !g_useThrottlePositionPtr || !g_throttleSettingsPtr || !g_pressureTimeMapPtr || !g_lockupSettingsPtr || !g_invertDriverOutputsPtr)
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
  *g_invertDriverOutputsPtr = g_defaultInvertDriverOutputs;
  // "Reset to defaults" also turns the gear lever override back off (rather than leaving a debug
  // override engaged) - this is persisted below along with everything else.
  g_gearLeverOverrideEnabled = false;
  g_overrideLeverPosition = VaLas_Controller::GearLeverPosition::Drive;

  // Save to SPIFFS
  shiftConfig.SaveConfig(g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr, g_useLargeDisplayPtr, g_useThrottlePositionPtr, g_throttleSettingsPtr, g_pressureTimeMapPtr, g_lockupSettingsPtr, g_invertDriverOutputsPtr);

  Serial.println("[WEB] Reset action received");
  Serial.print("[WEB] useCanBus: "); Serial.println(*g_useCanBusPtr);
  Serial.print("[WEB] usePedalShifters: "); Serial.println(*g_usePedalShiftersPtr);
  Serial.print("[WEB] useLargeDisplay: "); Serial.println(*g_useLargeDisplayPtr);
  Serial.print("[WEB] useThrottlePosition: "); Serial.println(*g_useThrottlePositionPtr);
  Serial.print("[WEB] useInvertDriverOutputs: "); Serial.println(*g_invertDriverOutputsPtr);
  Serial.print("[WEB] gearLeverOverride: "); Serial.print(g_gearLeverOverrideEnabled ? "ON" : "OFF");
  Serial.print(", position="); Serial.println(overridePositionName(g_overrideLeverPosition));
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

// Shared by handleSave() and handleOverrideSave(): reads a posted P/R/N/D-position field (if
// present) and applies it to g_overrideLeverPosition. Returns true if a recognised value was found.
static bool applyOverridePositionArg(const String& argName)
{
  if (!webServer.hasArg(argName))
    return false;

  String pos = webServer.arg(argName);
  if (pos == "Park") g_overrideLeverPosition = VaLas_Controller::GearLeverPosition::Park;
  else if (pos == "Reverse") g_overrideLeverPosition = VaLas_Controller::GearLeverPosition::Reverse;
  else if (pos == "Neutral") g_overrideLeverPosition = VaLas_Controller::GearLeverPosition::Neutral;
  else if (pos == "Drive") g_overrideLeverPosition = VaLas_Controller::GearLeverPosition::Drive;
  else return false;

  return true;
}

// Human-readable name for Serial logging; matches the <option> values used in the override dropdown.
static const char* overridePositionName(VaLas_Controller::GearLeverPosition pos)
{
  switch (pos)
  {
    case VaLas_Controller::GearLeverPosition::Park: return "Park";
    case VaLas_Controller::GearLeverPosition::Reverse: return "Reverse";
    case VaLas_Controller::GearLeverPosition::Neutral: return "Neutral";
    case VaLas_Controller::GearLeverPosition::Drive: return "Drive";
    default: return "Unknown";
  }
}

static void handleSave()
{
  if (!g_shiftSettingsPtr || !g_useCanBusPtr || !g_usePedalShiftersPtr || !g_useLargeDisplayPtr || !g_useThrottlePositionPtr || !g_throttleSettingsPtr || !g_pressureTimeMapPtr || !g_lockupSettingsPtr || !g_invertDriverOutputsPtr)
  {
    sendAndClose(500, "text/plain", "Configuration not initialised yet");
    return;
  }

  // Checkboxes: only present if checked
  *g_useCanBusPtr = webServer.hasArg("useCanBus");
  *g_usePedalShiftersPtr = webServer.hasArg("usePedalShifters");
  *g_useLargeDisplayPtr = webServer.hasArg("useLargeDisplay");
  *g_useThrottlePositionPtr = webServer.hasArg("useThrottlePosition");
  *g_invertDriverOutputsPtr = webServer.hasArg("invertDriverOutputs");
  // Gear lever override: this normally applies instantly via its own onchange -> /override POST,
  // but that's a fire-and-forget fetch that can be lost if the page navigates away (e.g. clicking
  // Save right after flipping it) before the ESP32 has processed it. Re-applying it here from the
  // same fields, now part of this form, makes a Save always land the override state too.
  g_gearLeverOverrideEnabled = webServer.hasArg("gearLeverOverrideEnabled");
  applyOverridePositionArg("gearLeverOverridePosition");
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
  shiftConfig.SaveConfig(g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr, g_useLargeDisplayPtr, g_useThrottlePositionPtr, g_throttleSettingsPtr, g_pressureTimeMapPtr, g_lockupSettingsPtr, g_invertDriverOutputsPtr);
  Serial.println("[WEB] Save action received");
  Serial.print("[WEB] useCanBus: "); Serial.println(*g_useCanBusPtr);
  Serial.print("[WEB] usePedalShifters: "); Serial.println(*g_usePedalShiftersPtr);
  Serial.print("[WEB] useLargeDisplay: "); Serial.println(*g_useLargeDisplayPtr);
  Serial.print("[WEB] useThrottlePosition: "); Serial.println(*g_useThrottlePositionPtr);
  Serial.print("[WEB] useInvertDriverOutputs: "); Serial.println(*g_invertDriverOutputsPtr);
  Serial.print("[WEB] gearLeverOverride: "); Serial.print(g_gearLeverOverrideEnabled ? "ON" : "OFF");
  Serial.print(", position="); Serial.println(overridePositionName(g_overrideLeverPosition));
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
  if (!g_shiftSettingsPtr || !g_useCanBusPtr || !g_usePedalShiftersPtr || !g_useLargeDisplayPtr || !g_useThrottlePositionPtr || !g_throttleSettingsPtr || !g_pressureTimeMapPtr || !g_lockupSettingsPtr || !g_invertDriverOutputsPtr)
  {
    sendAndClose(500, "text/plain", "Configuration not initialised yet");
    return;
  }

  static StaticJsonDocument<3072> doc;
  shiftConfig.createJsonFromObject(
    g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr, g_useLargeDisplayPtr, g_useThrottlePositionPtr, g_throttleSettingsPtr, g_pressureTimeMapPtr, g_lockupSettingsPtr, g_invertDriverOutputsPtr, doc);
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
  if (!g_shiftSettingsPtr || !g_useCanBusPtr || !g_usePedalShiftersPtr || !g_useLargeDisplayPtr || !g_useThrottlePositionPtr || !g_throttleSettingsPtr || !g_pressureTimeMapPtr || !g_lockupSettingsPtr || !g_invertDriverOutputsPtr)
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

  ShiftConfig::createObjectFromJson(g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr, g_useLargeDisplayPtr, g_useThrottlePositionPtr, g_throttleSettingsPtr, g_pressureTimeMapPtr, g_lockupSettingsPtr, g_invertDriverOutputsPtr, doc);
  if (!shiftConfig.writeConfigToFile(g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr, g_useLargeDisplayPtr, g_useThrottlePositionPtr, g_throttleSettingsPtr, g_pressureTimeMapPtr, g_lockupSettingsPtr, g_invertDriverOutputsPtr))
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
  shiftConfig.SaveConfig(g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr, g_useLargeDisplayPtr, g_useThrottlePositionPtr, g_throttleSettingsPtr, g_pressureTimeMapPtr, g_lockupSettingsPtr, g_invertDriverOutputsPtr);
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
  shiftConfig.SaveConfig(g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr, g_useLargeDisplayPtr, g_useThrottlePositionPtr, g_throttleSettingsPtr, g_pressureTimeMapPtr, g_lockupSettingsPtr, g_invertDriverOutputsPtr);
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
  String csv = String(sensors.GetLastAtfTempC()) + "," + String(sensors.IsLastAtfValid() ? 1 : 0) + "," + String(sensors.GetLastAtfRawAdc());
  sendAndClose(200, "text/plain", csv);
}

static void handleOverrideSave()
{
  if (webServer.hasArg("enabled"))
    g_gearLeverOverrideEnabled = webServer.arg("enabled").toInt() != 0;

  applyOverridePositionArg("position");

  Serial.print("[WEB] Gear lever override: "); Serial.print(g_gearLeverOverrideEnabled ? "ON" : "OFF");
  Serial.print(", position="); Serial.println(overridePositionName(g_overrideLeverPosition));

  // Persist immediately (not just on the main Save button) so this instant-apply toggle survives
  // a reboot too, matching the rest of the config.
  if (g_shiftSettingsPtr)
    shiftConfig.SaveConfig(g_shiftSettingsPtr, g_useCanBusPtr, g_usePedalShiftersPtr, g_useLargeDisplayPtr, g_useThrottlePositionPtr, g_throttleSettingsPtr, g_pressureTimeMapPtr, g_lockupSettingsPtr, g_invertDriverOutputsPtr);

  sendAndClose(200, "text/plain", "OK");
}

// Shared by handleOverrideUpshift()/handleOverrideDownshift(): validates the override is enabled
// and in Drive/Reverse, logs the outcome either way, and queues the manual shift request if valid.
static void handleOverrideShiftRequest(VaLas_Controller::ShiftRequest request, const char* requestName)
{
  Serial.print("[WEB] /override/"); Serial.print(requestName); Serial.println(" received");

  if (!g_gearLeverOverrideEnabled)
  {
    Serial.println("[WEB] Gear lever override: rejected - override not enabled");
    sendAndClose(400, "text/plain", "Gear lever override is not enabled");
    return;
  }
  if (g_overrideLeverPosition != VaLas_Controller::GearLeverPosition::Drive && g_overrideLeverPosition != VaLas_Controller::GearLeverPosition::Reverse)
  {
    // ShiftControl/V2 only ever complete a shift request while in Drive/Reverse (see their own
    // "Only process shift requests in Drive or Reverse" guard) - queuing one here regardless would
    // get silently stuck (never consumed, never completed) and block all further overrides until
    // the lever position is changed, so reject it up front with a clear reason instead.
    Serial.print("[WEB] Gear lever override: rejected - lever position is "); Serial.println(overridePositionName(g_overrideLeverPosition));
    sendAndClose(400, "text/plain", "Lever position must be Drive or Reverse to shift");
    return;
  }
  g_overrideManualShiftRequest = request;
  Serial.print("[WEB] Gear lever override: manual "); Serial.print(requestName); Serial.println(" requested");
  sendAndClose(200, "text/plain", "OK");
}

static void handleOverrideUpshift()
{
  handleOverrideShiftRequest(VaLas_Controller::ShiftRequest::UpShift, "upshift");
}

static void handleOverrideDownshift()
{
  handleOverrideShiftRequest(VaLas_Controller::ShiftRequest::DownShift, "downshift");
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
  g_defaultInvertDriverOutputs = true;
}

bool getDisplayIsLarge() { // true = 128x64, false = 128x32 (0.96" vs 0.91")
  return g_useLargeDisplayPtr ? *g_useLargeDisplayPtr : false;
}

// Gear lever override (debug, see the "Debug:" web UI section). Called from gearLeverHandlerTask
// in the main .ino *instead of* gearLeverInterface->ReadGearLever() while active, so a flaky CAN
// bus is bypassed entirely for the reported lever position. The override's enabled/position state
// is persisted and can therefore already be active from the very first tick after boot - safe to
// skip the real ReadGearLever() call here regardless, because gearLeverHandlerTask always makes
// one unconditional priming call to it before its main loop starts (see VaLas_Controller.ino),
// which wires up gearLeverInterface's internal CompleteShiftRequest()/Reset() pointers before this
// function can ever run. Without that priming call, those pointers would stay null and the first
// CompleteShiftRequest() call from ShiftControl would dereference null and crash.
bool IsGearLeverOverrideActive()
{
  return g_gearLeverOverrideEnabled;
}

void ApplyGearLeverOverride(TaskStructs::GearLeverParameters* params)
{
  static int lastUpShiftState = HIGH;
  static int lastDownShiftState = HIGH;
  static bool loggedDeferred = false;

  *(params->oldLeverPositionPtr) = *(params->currentLeverPositionPtr);
  *(params->currentLeverPositionPtr) = g_overrideLeverPosition;

  // Physical up/down shift buttons (steering wheel / pedal shifters) are read directly here, the
  // same edge-triggered way Gearlever_Modded does, so they keep working even though a flaky CAN
  // bus can't be trusted to relay them via the normal gear lever path.
  int upShiftState = digitalRead(upShiftPin);
  int downShiftState = digitalRead(downShiftPin);
  bool upShiftPressed = (upShiftState == LOW && lastUpShiftState == HIGH);
  bool downShiftPressed = (downShiftState == LOW && lastDownShiftState == HIGH);
  lastUpShiftState = upShiftState;
  lastDownShiftState = downShiftState;

  if (*(params->currentShiftRequestPtr) != VaLas_Controller::ShiftRequest::NoShift)
  {
    // ShiftControl hasn't completed the previous request yet. Log once (not every ~100ms tick) so
    // a queued manual web request shows up as "waiting" instead of silently appearing to do nothing.
    if (g_overrideManualShiftRequest != VaLas_Controller::ShiftRequest::NoShift && !loggedDeferred)
    {
      Serial.println("[Override] Manual web shift button is queued, waiting for the current shift to complete");
      loggedDeferred = true;
    }
    return;
  }
  loggedDeferred = false;

  if (upShiftPressed)
  {
    *(params->currentShiftRequestPtr) = VaLas_Controller::ShiftRequest::UpShift;
    Serial.println("[Override] Upshift button pressed");
  }
  else if (downShiftPressed)
  {
    *(params->currentShiftRequestPtr) = VaLas_Controller::ShiftRequest::DownShift;
    Serial.println("[Override] Downshift button pressed");
  }
  else if (g_overrideManualShiftRequest != VaLas_Controller::ShiftRequest::NoShift)
  {
    *(params->currentShiftRequestPtr) = g_overrideManualShiftRequest;
    g_overrideManualShiftRequest = VaLas_Controller::ShiftRequest::NoShift;
    Serial.println("[Override] Manual web shift button applied");
  }
}
