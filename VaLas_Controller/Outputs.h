#ifndef OUTPUTS_H
#define OUTPUTS_H

#include "VaLas_Controller.h"

class Outputs {
    
	public:
		Outputs();
        void ToggleElrHighIdle();
		bool IsStartAllowed(VaLas_Controller::GearLeverPosition currentLeverPosition);
		
	private:
};
#endif
