
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
#include <SPI.h>
#include "Gearlever_CAN.h"
#include "VaLas_Controller.h"
#include "CAN_Lib/mcp2515.h"

int canValue = -1;
struct can_frame canMsg;
MCP2515 mcp2515(9); // Pin 9

Gearlever_CAN::Gearlever_CAN()
{
  SPI.begin();
  
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();
}

void Gearlever_CAN::ReadGearLever(VaLas_Controller::ShiftRequest& currentShiftRequest, VaLas_Controller::GearLeverPosition& currentLeverPosition)
{
  currentShiftRequest = VaLas_Controller::ShiftRequest::NoShift;

  readCanBus();

  switch (canValue)
  {
	case 5:
		currentLeverPosition = VaLas_Controller::GearLeverPosition::Drive;
		break;
	case 6:
		currentLeverPosition = VaLas_Controller::GearLeverPosition::Neutral;
		break;
	case 7:
		currentLeverPosition = VaLas_Controller::GearLeverPosition::Reverse;
		break;
	case 8:
		currentLeverPosition = VaLas_Controller::GearLeverPosition::Park;
		break;
	case 9:
		currentShiftRequest = VaLas_Controller::ShiftRequest::UpShift;
		break;
	case 10:
		currentShiftRequest = VaLas_Controller::ShiftRequest::DownShift;
		break;
	
	default: // I guess something went wrong...
		break;
  }
}

void Gearlever_CAN::Reset()
{
  // Not much to do here
  // Maybe clear canValue
  canValue = -1;
}

void Gearlever_CAN::readCanBus()
{
  // Do the actual CAN Bus reading here

// Read all can data
//   if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
//       char buffer[7];
//       Serial.print("FRAME:ID=");
//       Serial.print(canMsg.can_id);
//       Serial.print(":LEN=");
//       Serial.print(canMsg.can_dlc);
//       char tmp[3];
//       for (int i = 0; i<canMsg.can_dlc; i++)  {  // print the data
//         char buffer[5];
//         Serial.print(":");
//         sprintf(buffer,"%02X", canMsg.data[i]);
//         Serial.print(buffer);
//       }
//       Serial.println();
//   }


  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK)
  {
	if (canMsg.can_id == 0x00000230) // This is our shifter message  // Or 560? 
	{
	  canValue = canMsg.data[4]; // Read offset 4 > should contain the shifter position
		// /** Gets gear selector lever position (NAG only) */
		// EWM_230h_WHC get_WHC() const { return (EWM_230h_WHC)(raw >> 56 & 0xf); }
	}
  }

  //delay(50);
}
