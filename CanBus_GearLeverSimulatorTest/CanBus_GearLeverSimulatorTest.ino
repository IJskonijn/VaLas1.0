/*
 * CAN Bus Gear Lever Simulator Test
 * 
 * Cycles through P, R, N, D with shifting:
 * - P (Park) - 3s
 * - R (Reverse) - 3s
 * - N (Neutral) - 3s
 * - D (Drive) - Start at gear 2:
 *   - Send Upshift (value 9) until reaching 5+
 *   - Send Downshift (value 10) down to 1
 * - Return to P and repeat
 * 
 * Uses same library as VaLas_Controller: mcp_can.h
 * CAN ID: 0x230
 * Values: 5=Drive, 6=Neutral, 7=Reverse, 8=Park, 9=Upshift, 10=Downshift
 */

#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>

#define CAN_ID 0x230
#define SPI_CS_PIN 5

MCP_CAN CAN(SPI_CS_PIN);

// CAN values
#define VAL_DRIVE       5
#define VAL_NEUTRAL     6
#define VAL_REVERSE     7
#define VAL_PARK        8
#define VAL_UPSHIFT     9
#define VAL_DOWNSHIFT   10

// Simulation states
enum State {
  PARK,
  REVERSE,
  NEUTRAL,
  DRIVE_2,
  DRIVE_CENTER_1,
  UPSHIFT_TO_3,
  DRIVE_CENTER_2,
  UPSHIFT_TO_4,
  DRIVE_CENTER_3,
  UPSHIFT_TO_5,
  DRIVE_CENTER_4,
  DOWNSHIFT_TO_4,
  DRIVE_CENTER_5,
  DOWNSHIFT_TO_3,
  DRIVE_CENTER_6,
  DOWNSHIFT_TO_2,
  DRIVE_CENTER_7,
  DOWNSHIFT_TO_1
};

State currentState = PARK;
unsigned long lastStateChangeTime = 0;
const unsigned long INTERVAL_MS = 3000;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\nGear Lever Simulator - Starting");
  Serial.println("================================\n");
  
  if(CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("MCP2515 Initialized Successfully!");
    Serial.println("CAN ID: 0x230 @ 500kbps\n");
  } else {
    Serial.println("Error Initializing MCP2515...");
  }
  
  CAN.setMode(MCP_NORMAL);
  
  lastStateChangeTime = millis();
}

void loop() {
  if (millis() - lastStateChangeTime >= INTERVAL_MS) {
    lastStateChangeTime = millis();
    advanceState();
  }
  
  sendCAN();
  delay(500);
}

void advanceState() {
  switch (currentState) {
    case PARK:
      currentState = REVERSE;
      Serial.println("→ Reverse");
      break;
    case REVERSE:
      currentState = NEUTRAL;
      Serial.println("→ Neutral");
      break;
    case NEUTRAL:
      currentState = DRIVE_2;
      Serial.println("→ Drive (Gear 2)");
      break;
    case DRIVE_2:
      currentState = UPSHIFT_TO_3;
      Serial.println("→ Upshift to Gear 3");
      break;
    case UPSHIFT_TO_3:
      currentState = DRIVE_CENTER_1;
      Serial.println("→ Drive (center - releasing lever)");
      break;
    case DRIVE_CENTER_1:
      currentState = UPSHIFT_TO_4;
      Serial.println("→ Upshift to Gear 4");
      break;
    case UPSHIFT_TO_4:
      currentState = DRIVE_CENTER_2;
      Serial.println("→ Drive (center - releasing lever)");
      break;
    case DRIVE_CENTER_2:
      currentState = UPSHIFT_TO_5;
      Serial.println("→ Upshift to Gear 5");
      break;
    case UPSHIFT_TO_5:
      currentState = DRIVE_CENTER_3;
      Serial.println("→ Drive (center - releasing lever)");
      break;
    case DRIVE_CENTER_3:
      currentState = DOWNSHIFT_TO_4;
      Serial.println("→ Downshift to Gear 4");
      break;
    case DOWNSHIFT_TO_4:
      currentState = DRIVE_CENTER_4;
      Serial.println("→ Drive (center - releasing lever)");
      break;
    case DRIVE_CENTER_4:
      currentState = DOWNSHIFT_TO_3;
      Serial.println("→ Downshift to Gear 3");
      break;
    case DOWNSHIFT_TO_3:
      currentState = DRIVE_CENTER_5;
      Serial.println("→ Drive (center - releasing lever)");
      break;
    case DRIVE_CENTER_5:
      currentState = DOWNSHIFT_TO_2;
      Serial.println("→ Downshift to Gear 2");
      break;
    case DOWNSHIFT_TO_2:
      currentState = DRIVE_CENTER_6;
      Serial.println("→ Drive (center - releasing lever)");
      break;
    case DRIVE_CENTER_6:
      currentState = DOWNSHIFT_TO_1;
      Serial.println("→ Downshift to Gear 1");
      break;
    case DOWNSHIFT_TO_1:
      currentState = DRIVE_CENTER_7;
      Serial.println("→ Drive (center - releasing lever)");
      break;
    case DRIVE_CENTER_7:
      currentState = PARK;
      Serial.println("→ Park (cycle complete)\n");
      break;
  }
}

void sendCAN() {
  uint8_t value = 0;
  const char* label = "";
  
  switch (currentState) {
    case PARK:
      value = VAL_PARK;
      label = "PARK";
      break;
    case REVERSE:
      value = VAL_REVERSE;
      label = "REVERSE";
      break;
    case NEUTRAL:
      value = VAL_NEUTRAL;
      label = "NEUTRAL";
      break;
    case DRIVE_2:
      value = VAL_DRIVE;
      label = "DRIVE (G2)";
      break;
    case DRIVE_CENTER_1:
    case DRIVE_CENTER_2:
    case DRIVE_CENTER_3:
    case DRIVE_CENTER_4:
    case DRIVE_CENTER_5:
    case DRIVE_CENTER_6:
    case DRIVE_CENTER_7:
      value = VAL_DRIVE;
      label = "DRIVE (center)";
      break;
    case UPSHIFT_TO_3:
    case UPSHIFT_TO_4:
    case UPSHIFT_TO_5:
      value = VAL_UPSHIFT;
      label = "UPSHIFT (+)";
      break;
    case DOWNSHIFT_TO_4:
    case DOWNSHIFT_TO_3:
    case DOWNSHIFT_TO_2:
    case DOWNSHIFT_TO_1:
      value = VAL_DOWNSHIFT;
      label = "DOWNSHIFT (-)";
      break;
  }
  
  unsigned char data[1] = {value};
  CAN.sendMsgBuf(CAN_ID, 0, 1, data);
  
  Serial.print("[0x");
  Serial.print(value, HEX);
  Serial.print("] ");
  Serial.println(label);
}
