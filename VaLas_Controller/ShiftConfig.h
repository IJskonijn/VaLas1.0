#ifndef SHIFTCONFIG_H
#define SHIFTCONFIG_H

#include <ArduinoJson.h>
#include "VaLas_Controller.h"

class ShiftConfig {
    
	public:
		ShiftConfig();
        void ReceiveConfigViaBluetooth(VaLas_Controller::ShiftSetting (&shiftSettings)[6], bool& useCanBus);
        void SendConfigViaBluetooth(VaLas_Controller::ShiftSetting (&shiftSettings)[6], bool& useCanBus);
		void CreateDefaultConfig(VaLas_Controller::ShiftSetting (&shiftSettings)[6]);
		bool WriteConfigToFile(VaLas_Controller::ShiftSetting (&shiftSettings)[6], bool& useCanBus);
		bool LoadConfigFromFile(VaLas_Controller::ShiftSetting (&shiftSettings)[6], bool& useCanBus);
		
	private:
		void createDefaultConfig(VaLas_Controller::ShiftSetting (&shiftSettings)[6]);
		void createObjectFromJson(VaLas_Controller::ShiftSetting (&shiftSettings)[6], bool& useCanBus, StaticJsonDocument<385> doc);
		StaticJsonDocument<512> createJsonFromObject(VaLas_Controller::ShiftSetting (&shiftSettings)[6], bool& useCanBus);
};
#endif
