#ifndef SHIFTCONFIG_H
#define SHIFTCONFIG_H

#include <ArduinoJson.h>
#include "VaLas_Controller.h"

class ShiftConfig {
    
	public:
		ShiftConfig();
		void init();
		void execute(void * parameter);
		void ReceiveConfigViaBluetooth(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, const String& payload);
        void SendConfigViaBluetooth(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr);
		void LoadDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr);
		
	private:
		void test(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, String message);
		uint16_t calculateCrc16(const String& data);
		bool tryParseFramedPayload(const String& message, String& jsonOut);
		String generatedJsonWithApp();
		bool writeConfigToFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr);
		bool loadConfigFromFile(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr);
		void createDefaultConfig(VaLas_Controller::ShiftSetting* shiftSettings);
		void createObjectFromJson(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr, StaticJsonDocument<2048> doc);
		StaticJsonDocument<2048> createJsonFromObject(VaLas_Controller::ShiftSetting* shiftSettingsPtr, bool* useCanBusPtr, bool* usePedalShiftersPtr);
};
#endif
