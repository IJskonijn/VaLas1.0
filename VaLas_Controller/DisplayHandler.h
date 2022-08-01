#include "VaLas_Controller.h"

#ifndef DISPLAYHANDLER_H
#define DISPLAYHANDLER_H

class DisplayHandler {
    
	public:
		DisplayHandler();
		void DisplayStartupOnScreen();
		void DisplayOnScreen(const char* stringToDisplay);
        void DisplayOnScreen(const char* stringToDisplay, VaLas_Controller::GearLeverPosition currentLeverPosition, int atfTemp);
		const String ToString(VaLas_Controller::GearLeverPosition v);
		
	private:
};
#endif
