
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

#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>
#include "Gearlever_CAN.h"
#include "TaskStructs.h"
#include "Gearlever_Modded.h"

int canValue = -1;

VaLas_Controller::ShiftRequest* currentShiftRequestCanValue;
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

  // Configuring pin for /INT input
  pinMode(canInt, INPUT);
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
		*currentLeverPosition = VaLas_Controller::GearLeverPosition::Drive;
    if (usePedalShifters)
      *currentShiftRequestCanValue = pedalShiftGearLeverInterface->GetShiftRequest(currentLeverPosition);
		break;
	case 6:
		*currentLeverPosition = VaLas_Controller::GearLeverPosition::Neutral;
		break;
	case 7:
		*currentLeverPosition = VaLas_Controller::GearLeverPosition::Reverse;
    if (usePedalShifters)
      *currentShiftRequestCanValue = pedalShiftGearLeverInterface->GetShiftRequest(currentLeverPosition);
		break;
	case 8:
		*currentLeverPosition = VaLas_Controller::GearLeverPosition::Park;
		break;
	case 9:
		*currentShiftRequestCanValue = VaLas_Controller::ShiftRequest::UpShift;
		break;
	case 10:
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
  //if(!digitalRead(canInt))                         // If CAN_INT pin is low, read receive buffer
  //{    
    // if((rxId & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
    //   sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
    // else
    //   sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);
  
    // Serial.print(msgString);
  
    // if((rxId & 0x40000000) == 0x40000000){    // Determine if message is a remote request frame.
    //   sprintf(msgString, " REMOTE REQUEST FRAME");
    //   Serial.print(msgString);
    // } else {
    //   for(byte i = 0; i<len; i++){
    //     sprintf(msgString, " 0x%.2X", rxBuf[i]);
    //     Serial.print(msgString);
    //   }
    // }
        
    // Serial.println();


    // if (CAN.readMessage(&canMsg) == MCP2515::ERROR_OK)
    // {
    // if (canMsg.can_id == 0x00000230) // This is our shifter message  // Or 560? 
    // {
    //   canValue = canMsg.data[4]; // Read offset 4 > should contain the shifter position
    //   // /** Gets gear selector lever position (NAG only) */
    //   // EWM_230h_WHC get_WHC() const { return (EWM_230h_WHC)(raw >> 56 & 0xf); }
    // }
    // }

    Serial.println("ReadCanBus method");
    // if (CAN_MSGAVAIL == CAN.checkReceive())
    // {
      CAN.readMsgBuf(&rxId, &len, rxBuf);
      Serial.println("Read CanBus buffer");
      Serial.println("buffer lenght" + String(len));

      for(byte i = 0; i<len; i++){
        sprintf(msgString, " 0x%.2X", rxBuf[i]);
        Serial.print(msgString);
      }

      if (rxId == 0x230) // This is our shifter message
      {
        Serial.println("ID 230 msg found");
        canValue = rxBuf[4]; // Offset 4 bevat shifterpositie
        Serial.print("Shifterpositie: ");
        Serial.println(canValue);
      }
    //}

  delay(50);
  //}
}
