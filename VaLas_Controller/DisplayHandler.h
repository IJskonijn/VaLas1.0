#include "VaLas_Controller.h"
#include <U8g2lib.h>

#ifndef DISPLAYHANDLER_H
#define DISPLAYHANDLER_H

class DisplayHandler {
    
	public:
		DisplayHandler();
		void begin(VaLas_Controller::DisplayScreen* screenToDisplay);
		void execute(void * parameter);
		void DisplayStartupOnScreen();
		const String ToString(VaLas_Controller::GearLeverPosition leverPosition);
		const String ToString(VaLas_Controller::GearLeverPosition leverPosition, int currentGear);
		
	private:
		U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
		void displayMainScreen(VaLas_Controller::GearLeverPosition currentLeverPosition, int currentGear, int atfTemp);
		void displayShifting();
};
#endif
