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
#include "TaskStructs.h"
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
ShiftConfig shiftConfig;
Gearlever* gearLeverInterface;

bool initial_UseCanBus = false;
VaLas_Controller::ShiftSetting initial_GearboxSettings[6];
VaLas_Controller::ShiftSetting* initial_GearboxSettingsPtr = initial_GearboxSettings;

VaLas_Controller::DisplayScreen initial_screenToDisplay;

VaLas_Controller::GearLeverPosition initial_OldLeverPosition;
VaLas_Controller::GearLeverPosition initial_CurrentLeverPosition;
VaLas_Controller::ShiftRequest initial_CurrentShiftRequest;

int initial_Gear;
int initial_AtfTemp;
int initial_N2Rpm;
int initial_N3Rpm;
int initial_CalculatedRpm;
int initial_EngineRpm;

/////

TaskStructs::GearLeverParameters gearLeverParameters
{
  &initial_CurrentShiftRequest,
  &initial_CurrentLeverPosition,
  &initial_OldLeverPosition
};

TaskStructs::ShiftControlParameters shiftControlParameters
{
  &initial_Gear,
  &initial_CurrentLeverPosition,
  &initial_OldLeverPosition,
  &initial_CurrentShiftRequest,
  initial_GearboxSettingsPtr
};

TaskStructs::ShiftConfigParameters shiftConfigParameters
{
  &initial_UseCanBus,
  initial_GearboxSettingsPtr
};

TaskStructs::DisplayHandlerParameters displayHandlerParameters
{
  &initial_screenToDisplay,
  &initial_Gear,
  &initial_CurrentLeverPosition,
  &initial_CurrentShiftRequest,
  &initial_AtfTemp
};

TaskStructs::SensorParameters sensorParameters
{
  &initial_AtfTemp,
  &initial_N2Rpm,
  &initial_N3Rpm,
  &initial_CalculatedRpm
};

/////

void setup()
{
  Serial.begin(115200); // open the serial port at 9600 bps:
  Serial.write("Begin program");
  Serial.write("\n");
    initial_screenToDisplay = VaLas_Controller::DisplayScreen::Main;
  initial_OldLeverPosition = VaLas_Controller::GearLeverPosition::Unknown;
  initial_CurrentLeverPosition = VaLas_Controller::GearLeverPosition::Unknown;
  initial_CurrentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;
  initial_Gear = 2;  initial_AtfTemp = 0;
  initial_N2Rpm = 0;
  initial_N3Rpm = 0;
  initial_CalculatedRpm = 0;
  initial_EngineRpm = 0;

  displayHandler.begin();
  displayHandler.DisplayStartupOnScreen();
  // Initialize sensors
  sensors.init_sensors();
    // Configure engine RPM settings
  // Change these values based on your setup:
  // 
  // For OM603 engine (144 flywheel teeth):
  // VaLas_Controller::EngineType engineType = VaLas_Controller::EngineType::OM603;
  //
  // For OM606 engine (6 flywheel tabs):
  // VaLas_Controller::EngineType engineType = VaLas_Controller::EngineType::OM606;
  //
  // For Mercedes 300E RPM gauge (needs 3 pulses per RPM):
  // VaLas_Controller::RpmGaugeType gaugeType = VaLas_Controller::RpmGaugeType::Mercedes300E;
  //
  // For Mercedes 300D RPM gauge (needs 144 pulses per RPM):
  // VaLas_Controller::RpmGaugeType gaugeType = VaLas_Controller::RpmGaugeType::Mercedes300D;
  
  VaLas_Controller::EngineType engineType = VaLas_Controller::EngineType::OM606;  // Change this!
  VaLas_Controller::RpmGaugeType gaugeType = VaLas_Controller::RpmGaugeType::Mercedes300E;  // Change this!
  sensors.SetEngineConfig(engineType, gaugeType);

  shiftConfig.init();
  
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
  
  shiftConfig.LoadDefaultConfig(initial_GearboxSettingsPtr, &initial_UseCanBus);

  if (initial_UseCanBus)
    gearLeverInterface = new Gearlever_CAN();
  else
    gearLeverInterface = new Gearlever_Modded();

  shiftControl.init(&displayHandler, &pwmChannels, gearLeverInterface, &initial_screenToDisplay, initial_GearboxSettingsPtr);

  // Core 0 for critical
  xTaskCreatePinnedToCore(
    gearLeverHandlerTask,    // Function that should be called
    "GearLever Handler",   // Name of the task (for debugging)
    10000,            // Stack size (bytes)
    (void*) &gearLeverParameters, // Parameter to pass
    1,               // Task priority
    NULL,            // Task handle
    0                // Run on Core 0
  );

  xTaskCreatePinnedToCore(
    shiftControlHandlerTask,    // Function that should be called
    "Shiftcontrol Handler",   // Name of the task (for debugging)
    10000,            // Stack size (bytes)
    (void*) &shiftControlParameters, // Parameter to pass
    1,               // Task priority
    NULL,            // Task handle
    0                // Run on Core 0
  );

  // Core 1 for display and extra stuff
  xTaskCreatePinnedToCore(
    displayHandlerTask,    // Function that should be called
    "Display Handler",   // Name of the task (for debugging)
    10000,            // Stack size (bytes)
    (void*) &displayHandlerParameters, // Parameter to pass
    1,               // Task priority
    NULL,            // Task handle
    1                // Run on Core 1
  );
  xTaskCreatePinnedToCore(
    shiftConfigHandlerTask,    // Function that should be called
    "Shiftconfig Handler",   // Name of the task (for debugging)
    10000,            // Stack size (bytes)
    (void*) &shiftConfigParameters, // Parameter to pass
    1,               // Task priority
    NULL,            // Task handle
    1                // Run on Core 1
  );

  xTaskCreatePinnedToCore(
    sensorHandlerTask,    // Function that should be called
    "Sensor Handler",   // Name of the task (for debugging)
    10000,            // Stack size (bytes)
    (void*) &sensorParameters, // Parameter to pass
    1,               // Task priority
    NULL,            // Task handle
    1                // Run on Core 1
  );
}

void gearLeverHandlerTask(void* parameter){
  for(;;){
    gearLeverInterface->ReadGearLever(parameter);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void shiftControlHandlerTask(void* parameter){
  for(;;){
    shiftControl.execute(parameter);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void displayHandlerTask(void* parameter){
  for(;;){
    displayHandler.execute(parameter);
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

void shiftConfigHandlerTask(void* parameter){
  for(;;){
    shiftConfig.execute(parameter);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void sensorHandlerTask(void* parameter){
  TaskStructs::SensorParameters* params = (TaskStructs::SensorParameters*) parameter;
  
  // Sensor reading task - reads ATF temperature, transmission RPM, and engine RPM
  // Updates shared variables that are used by the display handler
  for(;;){
    // Read ATF temperature using the improved sensor reading function
    int atfTemp = 0;
    if (sensors.read_atf_temp(&atfTemp)) {
      *(params->atfTempPtr) = atfTemp;
    }
    
    // Read transmission RPM sensors (N2 and N3) and calculate transmission RPM
    int n2Rpm, n3Rpm, calcRpm;
    if (sensors.read_input_rpm(n2Rpm, n3Rpm, calcRpm, false)) {
      *(params->n2RpmPtr) = n2Rpm;
      *(params->n3RpmPtr) = n3Rpm;
      *(params->calculatedRpmPtr) = calcRpm;
    }
    
    // Read engine RPM and output signal to gauge
    int engineRpm = sensors.ReadEngineRpm();
    initial_EngineRpm = engineRpm; // Update global engine RPM variable
    
    vTaskDelay(500 / portTICK_PERIOD_MS); // Read sensors every 500ms
  }
}

// Everything is handled in tasks.
void loop(){ }
