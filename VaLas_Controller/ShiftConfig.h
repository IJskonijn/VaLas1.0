#ifndef SHIFTCONFIG_H
#define SHIFTCONFIG_H

#include <ArduinoJson.h>
#include "VaLas_Controller.h"

class ShiftConfig {
    
	public:
		ShiftConfig();
		void init();
		void execute(void * parameter);
		void LoadDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleSettings* throttleSettingsPtr);
		// Save the current configuration to SPIFFS (wrapper around the private writeConfigToFile).
		void SaveConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleSettings* throttleSettingsPtr);
		static void CreateDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettings);
		bool writeConfigToFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleSettings* throttleSettingsPtr);
		static void createObjectFromJson(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleSettings* throttleSettingsPtr, StaticJsonDocument<2048> doc);
		StaticJsonDocument<2048> createJsonFromObject(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleSettings* throttleSettingsPtr);
		bool getDisplayIsLarge();
		
	private:
		bool loadConfigFromFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, bool* useLargeDisplayPtr, bool* useThrottlePositionPtr, VaLas_Controller::ThrottleSettings* throttleSettingsPtr);
};
#endif
