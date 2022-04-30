#include "VaLas_Controller.h"
#include "Gearlever.h"

#ifndef GEARLEVER_CAN_H
#define GEARLEVER_CAN_H

class Gearlever_CAN : public Gearlever {
    
	public:
		Gearlever_CAN();
        virtual void ReadGearLever(VaLas_Controller::ShiftRequest& currentShiftRequest, VaLas_Controller::GearLeverPosition& currentLeverPosition);
		virtual void Reset();
		
	private:
		void readCanBus();
};
#endif
