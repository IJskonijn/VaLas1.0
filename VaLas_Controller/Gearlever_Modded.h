#include "VaLas_Controller.h"
#include "Gearlever.h"

#ifndef GEARLEVER_MODDED_H
#define GEARLEVER_MODDED_H

class Gearlever_Modded : public Gearlever {
    
	public:
		Gearlever_Modded();
        virtual void ReadGearLever(void * parameter);
		virtual void Reset();
		
	private:
		void readGearLeverPosition(VaLas_Controller::GearLeverPosition& currentLeverPosition);
		void readShiftRequest(VaLas_Controller::ShiftRequest& currentShiftRequest, VaLas_Controller::GearLeverPosition& currentLeverPosition);
};
#endif
