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
 * CAN ID: 0x230
 * Values: 5=Drive, 6=Neutral, 7=Reverse, 8=Park, 9=Upshift, 10=Downshift
 */

#include <mcp2515.h>

#define CAN_ID 0x230
#define CAN_SPEED MCP_8MHz_500kBPS

MCP2515 mcp2515(5);  // CS pin = GPIO 5

struct can_frame canMsg;

// CAN values from Gearlever_CAN.cpp
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
  UPSHIFT_TO_3,
  UPSHIFT_TO_4,
  UPSHIFT_TO_5,
  DOWNSHIFT_TO_4,
  DOWNSHIFT_TO_3,
  DOWNSHIFT_TO_2,
  DOWNSHIFT_TO_1
};

State currentState = PARK;
unsigned long lastStateChangeTime = 0;
const unsigned long INTERVAL_MS = 3000;
int currentGear = 2;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\nGear Lever Simulator - Starting");
  Serial.println("================================\n");
  
  SPI.begin(18, 19, 23, 5);  // CLK, MISO, MOSI, CS
  mcp2515.reset();
  mcp2515.setBitrate(CAN_SPEED);
  mcp2515.setNormalMode();
  
  Serial.println("CAN Initialized (0x230 @ 500kbps)\n");
  
  canMsg.can_id = CAN_ID;
  canMsg.can_dlc = 1;
  
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
      currentGear = 2;
      Serial.println("→ Drive (Gear 2)");
      break;
    case DRIVE_2:
      currentState = UPSHIFT_TO_3;
      currentGear = 3;
      Serial.println("→ Upshift to Gear 3");
      break;
    case UPSHIFT_TO_3:
      currentState = UPSHIFT_TO_4;
      currentGear = 4;
      Serial.println("→ Upshift to Gear 4");
      break;
    case UPSHIFT_TO_4:
      currentState = UPSHIFT_TO_5;
      currentGear = 5;
      Serial.println("→ Upshift to Gear 5");
      break;
    case UPSHIFT_TO_5:
      currentState = DOWNSHIFT_TO_4;
      currentGear = 4;
      Serial.println("→ Downshift to Gear 4");
      break;
    case DOWNSHIFT_TO_4:
      currentState = DOWNSHIFT_TO_3;
      currentGear = 3;
      Serial.println("→ Downshift to Gear 3");
      break;
    case DOWNSHIFT_TO_3:
      currentState = DOWNSHIFT_TO_2;
      currentGear = 2;
      Serial.println("→ Downshift to Gear 2");
      break;
    case DOWNSHIFT_TO_2:
      currentState = DOWNSHIFT_TO_1;
      currentGear = 1;
      Serial.println("→ Downshift to Gear 1");
      break;
    case DOWNSHIFT_TO_1:
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
    case UPSHIFT_TO_3:
    case UPSHIFT_TO_4:
    case UPSHIFT_TO_5:
      value = VAL_UPSHIFT;
      label = "UPSHIFT";
      break;
    case DOWNSHIFT_TO_4:
    case DOWNSHIFT_TO_3:
    case DOWNSHIFT_TO_2:
    case DOWNSHIFT_TO_1:
      value = VAL_DOWNSHIFT;
      label = "DOWNSHIFT";
      break;
  }
  
  canMsg.data[0] = value;
  mcp2515.sendMessage(&canMsg);
  
  Serial.print("[0x");
  Serial.print(value, HEX);
  Serial.print("] ");
  Serial.println(label);
}
