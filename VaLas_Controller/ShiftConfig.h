#ifndef SHIFTCONFIG_H
#define SHIFTCONFIG_H

#include "VaLas_Controller.h"

class ShiftConfig {
    
	public:
		ShiftConfig();
        void ReceiveConfigViaBluetooth(VaLas_Controller::ShiftSetting (&shiftSettings)[6], bool& useCanBus);
        void SendConfigViaBluetooth(VaLas_Controller::ShiftSetting (&shiftSettings)[6], bool& useCanBus);
		void CreateDefaultConfig(VaLas_Controller::ShiftSetting (&shiftSettings)[6]);
		
	private:
		void createDefaultConfig(VaLas_Controller::ShiftSetting (&shiftSettings)[6]);
};
#endif
