#ifndef SHIFTCONFIG_H
#define SHIFTCONFIG_H

#include "VaLas_Controller.h"

class ShiftConfig {
    
	public:
		ShiftConfig();
        void ReceiveConfigViaBluetooth();
        void SendConfigViaBluetooth();
		void CreateDefaultConfig(VaLas_Controller::ShiftSetting (&shiftSettings)[6]);
		
	private:
};
#endif
