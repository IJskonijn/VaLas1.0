#include "VaLas_Controller.h"
#include <U8g2lib.h>

#ifndef DISPLAYHANDLER_H
#define DISPLAYHANDLER_H

class DisplayHandler {
    
	public:
		DisplayHandler();
		void begin();
		void execute(void * parameter);
		void DisplayStartupOnScreen();
		const String ToString(const VaLas_Controller::GearLeverPosition leverPosition);
		const String ToString(const VaLas_Controller::GearLeverPosition leverPosition, const int currentGear);
		
	private:
		int u8g2_y_coordinate;
		const uint8_t* u8g2_selectedFont;

	#ifdef is_096_Oled
		U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
	#else
		U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2;
	#endif

		void displayMainScreen(const VaLas_Controller::GearLeverPosition currentLeverPosition, const int currentGear, const int atfTemp);
		void displayShifting();
};
#endif
