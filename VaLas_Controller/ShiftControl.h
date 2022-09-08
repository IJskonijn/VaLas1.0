#include "VaLas_Controller.h"
#include "DisplayHandler.h"

#ifndef SHIFTCONTROL_H
#define SHIFTCONTROL_H

class ShiftControl {
    
	public:
		void init(DisplayHandler* displayHandlerPtr, VaLas_Controller::PwmChannels* pwmChannelsPtr, Gearlever* gearLeverPtr, VaLas_Controller::DisplayScreen* screenToDisplayPtr);
		void execute(void * parameter);
		
	private:
		void processLeverValues();
		void resetToGear2();
		void downShift(int customMpcAfterShift);
		void upShift(int customMpcAfterShift);
		void select_fivetcc_to_five();
		void select_five_to_fivetcc();
};
#endif
