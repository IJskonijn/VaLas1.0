
// GEARLEVER SETTINGS
// FRAME EWM_230h (0x00000230)
// 	SIGNAL W_S, OFFSET: 0, LEN: 1, DESC: Driving program, DATA TYPE BOOL
// 	SIGNAL FPT, OFFSET: 1, LEN: 1, DESC: Driving program button actuated, DATA TYPE BOOL
// 	SIGNAL KD, OFFSET: 2, LEN: 1, DESC: Kickdown, DATA TYPE BOOL
// 	SIGNAL SPERR, OFFSET: 3, LEN: 1, DESC: barrier magnet energized, DATA TYPE BOOL
// 	SIGNAL WHC, OFFSET: 4, LEN: 4, DESC: gear selector lever position (NAG only), DATA TYPE ENUM
// 		ENUM D, RAW: 5, DESC: selector lever in position "D"
// 		ENUM N, RAW: 6, DESC: selector lever in position "N"
// 		ENUM R, RAW: 7, DESC: selector lever in position "R"
// 		ENUM P, RAW: 8, DESC: selector lever in position "P"
// 		ENUM PLUS, RAW: 9, DESC: selector lever in position "+"
// 		ENUM MINUS, RAW: 10, DESC: selector lever in position "-"
// 		ENUM N_ZW_D, RAW: 11, DESC: selector lever in intermediate position "N-D"
// 		ENUM R_ZW_N, RAW: 12, DESC: selector lever in intermediate position "R-N"
// 		ENUM P_ZW_R, RAW: 13, DESC: selector lever in intermediate position "P-R"
// 		ENUM SNV, RAW: 15, DESC: selector lever position unplausible

#include <Arduino.h>
#include "Gearlever_CAN.h"
#include "VaLas_Controller.h"

/** gear selector lever position (NAG only) */
enum class EWM_230h_WHC {
	D = 5, // selector lever in position "D"
	N = 6, // selector lever in position "N"
	R = 7, // selector lever in position "R"
	P = 8, // selector lever in position "P"
	PLUS = 9, // selector lever in position "+"
	MINUS = 10, // selector lever in position "-"
};

Gearlever_CAN::Gearlever_CAN()
{}

void Gearlever_CAN::ReadGearLever(VaLas_Controller::ShiftRequest& currentShiftRequest, VaLas_Controller::GearLeverPosition& currentLeverPosition)
{

}

void Gearlever_CAN::Reset()
{
  
}
