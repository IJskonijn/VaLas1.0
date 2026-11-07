#ifndef SHIFTCONFIG_H
#define SHIFTCONFIG_H

#include <ArduinoJson.h>
#include "VaLas_Controller.h"

class ShiftConfig {
    
	public:
		ShiftConfig();
		void init();
		void execute(void * parameter);
		void LoadDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr, bool* invertDriverOutputsPtr);
		// Save the current configuration to SPIFFS (wrapper around the private writeConfigToFile).
		void SaveConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr, bool* invertDriverOutputsPtr);
		static void CreateDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettings);
		bool writeConfigToFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr, bool* invertDriverOutputsPtr);
		static void createObjectFromJson(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr, bool* invertDriverOutputsPtr, const StaticJsonDocument<3072>& doc);
		void createJsonFromObject(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr, bool* invertDriverOutputsPtr, StaticJsonDocument<3072>& doc);
		bool getDisplayIsLarge();
		
	private:
		bool loadConfigFromFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleCalibration* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, VaLas_Controller::LockupControlSettings* lockupSettingsPtr, bool* invertDriverOutputsPtr);
};
#endif
