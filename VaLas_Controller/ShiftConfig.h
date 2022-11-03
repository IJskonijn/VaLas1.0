#ifndef SHIFTCONFIG_H
#define SHIFTCONFIG_H

#include <ArduinoJson.h>
#include "VaLas_Controller.h"

class ShiftConfig {
    
	public:
		ShiftConfig();
		void init();
		void execute(void * parameter);
        void ReceiveConfigViaBluetooth(VaLas_Controller::ShiftSetting* shiftsettingsptr, bool* usecanbusptr);
        void SendConfigViaBluetooth(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr);
		void LoadDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr);
		
	private:
		void test(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, String message);
		String generatedJsonWithApp();
		bool writeConfigToFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr);
		bool loadConfigFromFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr);
		void createDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettings);
		void createObjectFromJson(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, StaticJsonDocument<2048> doc);
		StaticJsonDocument<2048> createJsonFromObject(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr);
};
#endif
