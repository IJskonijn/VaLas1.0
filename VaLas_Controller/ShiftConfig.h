#ifndef SHIFTCONFIG_H
#define SHIFTCONFIG_H

#include <ArduinoJson.h>
#include "VaLas_Controller.h"

class ShiftConfig {
    
	public:
		ShiftConfig();
		void init();
		void execute(void * parameter);
		void LoadDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleSettings* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr);
		// Save the current configuration to SPIFFS (wrapper around the private writeConfigToFile).
		void SaveConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleSettings* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr);
		static void CreateDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettings);
		bool writeConfigToFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleSettings* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr);
		static void createObjectFromJson(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleSettings* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, const StaticJsonDocument<3072>& doc);
		void createJsonFromObject(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleSettings* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr, StaticJsonDocument<3072>& doc);
		bool getDisplayIsLarge();
		
	private:
		bool loadConfigFromFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleSettings* throttleSettingsPtr, VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr);
};
#endif
