#include "VaLas_Controller.h"

#ifndef GEARLEVER_H
#define GEARLEVER_H

class Gearlever {
    
	public:
        virtual void ReadGearLever(VaLas_Controller::ShiftRequest& currentShiftRequest, VaLas_Controller::GearLeverPosition& currentLeverPosition);
		virtual void Reset();
		
	private:
};
#endif
