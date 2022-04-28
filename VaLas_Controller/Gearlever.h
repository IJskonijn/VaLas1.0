#include "VaLas_Controller.h"

#ifndef GEARLEVER_H
#define GEARLEVER_H

class Gearlever {
    
	public:
		Gearlever();
        void ReadGearLeverPosition(VaLas_Controller::GearLeverPosition& currentLeverPosition);
        void ReadShiftRequest(VaLas_Controller::ShiftRequest& currentShiftRequest, VaLas_Controller::GearLeverPosition& currentLeverPosition);
		void Reset();
		
	private:
};
#endif
