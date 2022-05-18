
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
#include <mcp_can.h>
#include "Gearlever_CAN.h"
#include "VaLas_Controller.h"

int canValue = -1;

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];   

#define CAN_INT 2    // Set INT to pin 2
MCP_CAN CAN(10);     // Set CS to pin 10

Gearlever_CAN::Gearlever_CAN()
{
  Serial.println("Using CAN gearlever");

  // Initialize MCP2515 running at 8MHz with a baudrate of 500kb/s and the masks and filters disabled.
  if(CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK)
    Serial.println("MCP2515 Initialized Successfully!");
  else
    Serial.println("Error Initializing MCP2515...");
  
  // Set operation mode to normal so the MCP2515 sends acks to received data.
  CAN.setMode(MCP_NORMAL);

  // Configuring pin for /INT input
  pinMode(CAN_INT, INPUT);
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
  if(!digitalRead(CAN_INT))                         // If CAN_INT pin is low, read receive buffer
  {
    CAN.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
    
    if((rxId & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
    else
      sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);
  
    Serial.print(msgString);
  
    if((rxId & 0x40000000) == 0x40000000){    // Determine if message is a remote request frame.
      sprintf(msgString, " REMOTE REQUEST FRAME");
      Serial.print(msgString);
    } else {
      for(byte i = 0; i<len; i++){
        sprintf(msgString, " 0x%.2X", rxBuf[i]);
        Serial.print(msgString);
      }
    }
        
    Serial.println();
  }


//   if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK)
//   {
// 	if (canMsg.can_id == 0x00000230) // This is our shifter message  // Or 560? 
// 	{
// 	  canValue = canMsg.data[4]; // Read offset 4 > should contain the shifter position
// 		// /** Gets gear selector lever position (NAG only) */
// 		// EWM_230h_WHC get_WHC() const { return (EWM_230h_WHC)(raw >> 56 & 0xf); }
// 	}
//   }

  //delay(50);
}
