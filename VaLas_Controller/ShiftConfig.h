#ifndef SHIFTCONFIG_H
#define SHIFTCONFIG_H

#include <ArduinoJson.h>
#include "VaLas_Controller.h"

class ShiftConfig {
    
	public:
		ShiftConfig();
		void init();
		void execute(void * parameter);
		void LoadDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr);
		// Save the current configuration to SPIFFS (wrapper around the private writeConfigToFile).
		void SaveConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr);
		static void CreateDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettings);
		bool writeConfigToFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr);
		static void createObjectFromJson(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, StaticJsonDocument<2048> doc);
		StaticJsonDocument<2048> createJsonFromObject(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr);
		bool getDisplayIsLarge();
		
	private:
		bool loadConfigFromFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr);
};
#endif
