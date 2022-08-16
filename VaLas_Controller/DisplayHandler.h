#include "VaLas_Controller.h"

#ifndef DISPLAYHANDLER_H
#define DISPLAYHANDLER_H

class DisplayHandler {
    
	public:
		DisplayHandler();
		void execute(void * parameter);
		void DisplayStartupOnScreen();
		void DisplayOnScreen(String stringToDisplay);
		const String ToString(VaLas_Controller::GearLeverPosition v);
		const String ToString(VaLas_Controller::GearLeverPosition leverPosition, int currentGear);
		
	private:
};
#endif
