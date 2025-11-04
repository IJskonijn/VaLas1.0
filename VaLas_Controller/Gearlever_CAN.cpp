
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

// Found with Arduino:
// Standard ID: 0x230       DLC: 1  Data: 0x08  ==	8
// Standard ID: 0x230       DLC: 1  Data: 0x18  ==	24
// Standard ID: 0x230       DLC: 1  Data: 0x1D  ==	29
// Standard ID: 0x230       DLC: 1  Data: 0x17  ==	23
// Standard ID: 0x230       DLC: 1  Data: 0x07  ==	7
// Standard ID: 0x230       DLC: 1  Data: 0x0D  ==	13
// Standard ID: 0x230       DLC: 1  Data: 0x0C  ==	12
// Standard ID: 0x230       DLC: 1  Data: 0x06  ==	6
// Standard ID: 0x230       DLC: 1  Data: 0x0B  ==	11
// Standard ID: 0x230       DLC: 1  Data: 0x09  ==	9
// Standard ID: 0x230       DLC: 1  Data: 0x05  ==	5
// Standard ID: 0x230       DLC: 1  Data: 0x0A  ==	10


/*
// Readout with 5v + esp 3.3v
0x18	24 = P?
0x1D	29 = tussen R en P
0x07	7
0x0C	12
0x06	6
0x0B	11
0x09	9
0x05	5
0x0A	10
0x08	8

// Kijken naar winter mode, miss meer in array?
Winter mode (decimaal)
Printing gear on screen: 2
ATF: 0
Buff length: 1
Shifterpositie: 152
Shifterpositie: 157
Shifterpositie: 151
Shifterpositie: 135
Shifterpositie: 140
Shifterpositie: 134
Shifterpositie: 139
Shifterpositie: 137
Shifterpositie: 133
Shifterpositie: 138
Shifterpositie: 24
*/

#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>
#include "Gearlever_CAN.h"
#include "TaskStructs.h"
#include "Gearlever_Modded.h"

int canValue = -1;

VaLas_Controller::ShiftRequest* currentShiftRequestCanValue;
VaLas_Controller::ShiftRequest* oldShiftRequestCanValue;
Gearlever_Modded* pedalShiftGearLeverInterface;
bool usePedalShifters = false;

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];   

MCP_CAN CAN(spiCso);

Gearlever_CAN::Gearlever_CAN(bool* usePedalShiftersPtr)
{
  Serial.println("Using CAN gearlever");
  usePedalShifters = *usePedalShiftersPtr;

  if (usePedalShifters)
  {
    Serial.println("Using extra Modded gearlever logic for pedal shifters");
    pedalShiftGearLeverInterface = new Gearlever_Modded();
  }

  // Initialize MCP2515 running at 8MHz with a baudrate of 500kb/s and the masks and filters disabled.
  if(CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK)
    Serial.println("MCP2515 Initialized Successfully!");
  else
    Serial.println("Error Initializing MCP2515...");
  
  // Set operation mode to normal so the MCP2515 sends acks to received data.
  CAN.setMode(MCP_NORMAL);
}

void Gearlever_CAN::ReadGearLever(void * parameter)
{
  TaskStructs::GearLeverParameters *parameters = (TaskStructs::GearLeverParameters*) parameter;   
  VaLas_Controller::GearLeverPosition* currentLeverPosition = parameters->currentLeverPositionPtr;
  VaLas_Controller::GearLeverPosition* oldLeverPosition = parameters->oldLeverPositionPtr;
  currentShiftRequestCanValue = parameters->currentShiftRequestPtr;

  *oldLeverPosition = *currentLeverPosition;

  readCanBus();
  
  switch (canValue)
  {
	case 5:
	case 133:
		*currentLeverPosition = VaLas_Controller::GearLeverPosition::Drive;
    // if (usePedalShifters)
    //   *currentShiftRequestCanValue = pedalShiftGearLeverInterface->GetShiftRequest(currentLeverPosition);
		break;
	case 6:
	case 134:
		*currentLeverPosition = VaLas_Controller::GearLeverPosition::Neutral;
		break;
	case 7:
	case 135:
		*currentLeverPosition = VaLas_Controller::GearLeverPosition::Reverse;
    // if (usePedalShifters)
    //   *currentShiftRequestCanValue = pedalShiftGearLeverInterface->GetShiftRequest(currentLeverPosition);
		break;
	case 8:
	case 24:
	case 152:
		*currentLeverPosition = VaLas_Controller::GearLeverPosition::Park;
		break;
	case 9:
	case 137:
		*currentShiftRequestCanValue = VaLas_Controller::ShiftRequest::UpShift;
		break;
	case 10:
	case 138:
		*currentShiftRequestCanValue = VaLas_Controller::ShiftRequest::DownShift;
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

void Gearlever_CAN::CompleteShiftRequest()
{
  *currentShiftRequestCanValue = VaLas_Controller::ShiftRequest::NoShift;
}

void Gearlever_CAN::readCanBus()
{
  // Do the actual CAN Bus reading here

    CAN.readMsgBuf(&rxId, &len, rxBuf);

    // for(byte i = 0; i<len; i++){
    //   sprintf(msgString, " 0x%.2X", rxBuf[i]);
    //   Serial.println(msgString);
    // }

    if (rxId == 0x230) // This is our shifter message
    {
      canValue = (int)rxBuf[0]; // Offset 4 bevat shifterpositie
      Serial.println("Buff length: " + String(len));
      Serial.println("Shifterpositie: " + String(canValue));
    }

  delay(50);
}
