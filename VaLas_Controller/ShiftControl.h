#include "VaLas_Controller.h"
#include "DisplayHandler.h"

#ifndef SHIFTCONTROL_H
#define SHIFTCONTROL_H

class ShiftControl {
    
	public:
		void init(DisplayHandler* displayHandlerPtr, VaLas_Controller::PwmChannels* pwmChannelsPtr, Gearlever* gearLeverPtr, 
			VaLas_Controller::DisplayScreen* screenToDisplayPtr, VaLas_Controller::ShiftSetting* gearboxSettingsPtr);
		void execute(void * parameter);
		
	private:
		void processLeverValues(VaLas_Controller::GearLeverPosition oldLeverPosition, VaLas_Controller::GearLeverPosition currentLeverPosition, int* gear);
		void resetToGear2(VaLas_Controller::GearLeverPosition currentLeverPosition, int* gear);
		void downShift(int customMpcAfterShift, VaLas_Controller::GearLeverPosition currentLeverPosition, int gear);
		void upShift(int customMpcAfterShift, VaLas_Controller::GearLeverPosition currentLeverPosition, int gear);
		void select_fivetcc_to_five(VaLas_Controller::GearLeverPosition currentLeverPosition, int gear);
		void select_five_to_fivetcc(VaLas_Controller::GearLeverPosition currentLeverPosition, int gear);
};
#endif
