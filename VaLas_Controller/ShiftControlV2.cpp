#include <Arduino.h>
#include "TaskStructs.h"
#include "ShiftControlV2.h"
#include "ShiftConfig.h"
#include "Sensors.h"
#include "Gearlever.h"
#include "Gearlever_CAN.h"
#include "Gearlever_Modded.h"

VaLas_Controller::PwmChannels* pwmChannelsPointerV2;
DisplayHandler* displayHandlerPointerV2;
Gearlever* gearleverV2;

VaLas_Controller::DisplayScreen* screenToDisplayValueV2;
VaLas_Controller::ShiftSetting* gearboxSettingsV2;
int* throttlePositionPointerV2;
VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPointerV2;

void ShiftControlV2::init(DisplayHandler* displayHandlerPtr, VaLas_Controller::PwmChannels* pwmChannelsPtr, Gearlever* gearLeverPtr,
  VaLas_Controller::DisplayScreen* screenToDisplayPtr, VaLas_Controller::ShiftSetting* gearboxSettingsPtr)
{
  Serial.println("Init ShiftControlV2");
  displayHandlerPointerV2 = displayHandlerPtr;
  pwmChannelsPointerV2 = pwmChannelsPtr;
  gearleverV2 = gearLeverPtr;
  screenToDisplayValueV2 = screenToDisplayPtr;
  gearboxSettingsV2 = gearboxSettingsPtr;
  throttlePositionPointerV2 = nullptr;
  pressureTimeMapPointerV2 = nullptr;
}

void ShiftControlV2::execute(void * parameter)
{
  TaskStructs::ShiftControlParameters *parameters = (TaskStructs::ShiftControlParameters*) parameter;
  int* gear = parameters->gearPtr;
  throttlePositionPointerV2 = parameters->throttlePositionPtr;
  pressureTimeMapPointerV2 = parameters->pressureTimeMapPtr;
  int atfTempC = parameters->atfTempPtr ? *(parameters->atfTempPtr) : 90; // Fall back to a warmed-up assumption if not wired
  VaLas_Controller::GearLeverPosition oldLeverPosition = *(parameters->oldLeverPositionPtr);
  VaLas_Controller::GearLeverPosition currentLeverPosition = *(parameters->currentLeverPositionPtr);
  VaLas_Controller::ShiftRequest currentShiftRequest = *(parameters->currentShiftRequestPtr);

  // A shift is already in flight; advance it and ignore new lever/request changes, same re-entry safety as V1's blocking design.
  if (phase != Phase::Idle)
  {
    tick();
    return;
  }

  processLeverValues(oldLeverPosition, currentLeverPosition, gear);

  // Only process shift requests in Drive or Reverse
  if ((currentLeverPosition != VaLas_Controller::GearLeverPosition::Drive && currentLeverPosition != VaLas_Controller::GearLeverPosition::Reverse)
      || currentShiftRequest == VaLas_Controller::ShiftRequest::NoShift)
  {
    return; // Nothing to do if there is no shiftrequest
  }

  // Check for the up_shift in Drive
  if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Drive && currentShiftRequest == VaLas_Controller::ShiftRequest::UpShift)
  {
    Serial.println("Upshift detected");
    Serial.println("Current gear before upshift " + String(*gear));
    if ((*gear >= 1) && (*gear <= 5))
    {
      (*gear)++;
      Serial.println("Upshifting to " + String(*gear));

      switch (*gear)
      {
        case 2:
        case 3:
        case 4:
          startUpShift(0, currentLeverPosition, *gear, atfTempC);
          break;
        case 5:
          startUpShift(15, currentLeverPosition, *gear, atfTempC);
          break;
        case 6:
          startSelectFiveToFivetcc(currentLeverPosition, *gear, atfTempC);
          break;
        default:
          *gear = 6;
          return;
      }
    }

    gearleverV2->CompleteShiftRequest();
    Serial.println("Current gear after upshift" + String(*gear));
  }

  // check for the down_shift
  else if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Drive && currentShiftRequest == VaLas_Controller::ShiftRequest::DownShift)
  {
    Serial.println("Downshift detected");
    Serial.println("Current gear before downshift" + String(*gear));
    if ((*gear >= 2) && (*gear <= 6))
    {
      (*gear)--;
      Serial.println("Downshifting to " + String(*gear));

      switch (*gear)
      {
        case 2:
          startDownShift(20, currentLeverPosition, *gear, atfTempC);
          break;
        case 1:
        case 3:
        case 4:
          startDownShift(0, currentLeverPosition, *gear, atfTempC);
          break;
        case 5:
          startSelectFivetccToFive(currentLeverPosition, *gear, atfTempC);
          break;
        default:
          *gear = 1;
          return;
      }
    }

    gearleverV2->CompleteShiftRequest();
    Serial.println("Current gear after downshift" + String(*gear));
  }

  // Check for upshift in Reverse (R1 -> R2)
  else if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Reverse && currentShiftRequest == VaLas_Controller::ShiftRequest::UpShift)
  {
    Serial.println("Reverse upshift detected");
    if (*gear == 1)
    {
      *gear = 2;
      Serial.println("Upshifting to R2");
      // R1 -> R2 uses same shift as forward 1->2
      startUpShift(0, currentLeverPosition, *gear, atfTempC);
    }

    gearleverV2->CompleteShiftRequest();
  }

  // Check for downshift in Reverse (R2 -> R1)
  else if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Reverse && currentShiftRequest == VaLas_Controller::ShiftRequest::DownShift)
  {
    Serial.println("Reverse downshift detected");
    if (*gear == 2)
    {
      *gear = 1;
      Serial.println("Downshifting to R1");
      // R2 -> R1 uses same shift as forward 2->1
      startDownShift(20, currentLeverPosition, *gear, atfTempC);
    }

    gearleverV2->CompleteShiftRequest();
  }
}

void ShiftControlV2::processLeverValues(VaLas_Controller::GearLeverPosition oldLeverPosition, VaLas_Controller::GearLeverPosition currentLeverPosition, int* gear)
{
  if (currentLeverPosition == oldLeverPosition)
    return;

  resetToGear2(currentLeverPosition, gear);

  String printVar = displayHandlerPointerV2->ToString(currentLeverPosition, *gear) + " selected";
  Serial.println(printVar);
}

void ShiftControlV2::resetToGear2(VaLas_Controller::GearLeverPosition currentLeverPosition, int* gear)
{
  gearleverV2->Reset();
  gearleverV2->CompleteShiftRequest();

  if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Park || currentLeverPosition == VaLas_Controller::GearLeverPosition::Neutral)
  {
    ledcWrite(pwmChannelsPointerV2->mpcChannel, (255 * 40) / 100); //40%
    ledcWrite(pwmChannelsPointerV2->spcChannel, (255 * 33) / 100); //33%
    digitalWrite(y4Pin, LOW); // Back to idle
  }
  else
  {
    digitalWrite(y4Pin, LOW);
    ledcWrite(pwmChannelsPointerV2->spcChannel, 0); // Set to 0 in D and R
  }

  if (currentLeverPosition == VaLas_Controller::GearLeverPosition::Reverse || currentLeverPosition == VaLas_Controller::GearLeverPosition::Park)
    *gear = 2;
}

void ShiftControlV2::startDownShift(int customMpcAfterShift, VaLas_Controller::GearLeverPosition currentLeverPosition, int gear, int atfTempC)
{
  *screenToDisplayValueV2 = VaLas_Controller::DisplayScreen::Shifting;
  String screenVar = displayHandlerPointerV2->ToString(currentLeverPosition, gear);
  Serial.println("Downshift to " + screenVar);

  int gearPin = -1;
  if (gear == 1 || gear == 4)
    gearPin = y3Pin;
  else if (gear == 3)
    gearPin = y4Pin;
  else if (gear == 2)
    gearPin = y5Pin;
  else
    return; // Something went wrong

  int throttlePosition = throttlePositionPointerV2 ? *throttlePositionPointerV2 : 100;
  int linePressure = scalePressure2D(gearboxSettingsV2[gear].DownshiftLinePressure, throttlePosition, atfTempC);
  int shiftPressure = scalePressure2D(gearboxSettingsV2[gear].DownshiftShiftPressure, throttlePosition, atfTempC);
  unsigned long shiftDelay = scaleDelay2D(gearboxSettingsV2[gear].DownshiftDelay, throttlePosition, atfTempC);

  ledcWrite(pwmChannelsPointerV2->mpcChannel, linePressure);
  ledcWrite(pwmChannelsPointerV2->spcChannel, shiftPressure);
  digitalWrite(gearPin, HIGH);
  ledcWrite(pwmChannelsPointerV2->tccChannel, gearboxSettingsV2[gear].DownshiftTorqueConverterLockup);

  activeGearPin = gearPin;
  activeFinalMpc = scalePressure2D(customMpcAfterShift, throttlePosition, atfTempC);
  activeShiftDelayMs = shiftDelay;
  activeReducedLinePressure = linePressure / 2;
  activeReducedShiftPressure = shiftPressure / 2;
  activeKind = (gear == 2) ? TransitionKind::ThreeToTwoDownshift : TransitionKind::Normal;
  phase = Phase::Applying;
  phaseStartMs = millis();
}

void ShiftControlV2::startUpShift(int customMpcAfterShift, VaLas_Controller::GearLeverPosition currentLeverPosition, int gear, int atfTempC)
{
  *screenToDisplayValueV2 = VaLas_Controller::DisplayScreen::Shifting;
  String screenVar = displayHandlerPointerV2->ToString(currentLeverPosition, gear);
  Serial.println("Upshift to " + screenVar);

  int gearPin = -1;
  if (gear == 2 || gear == 5)
    gearPin = y3Pin;
  else if (gear == 4)
    gearPin = y4Pin;
  else if (gear == 3)
    gearPin = y5Pin;
  else
    return; // Something went wrong

  int throttlePosition = throttlePositionPointerV2 ? *throttlePositionPointerV2 : 100;
  int linePressure = scalePressure2D(gearboxSettingsV2[gear - 2].UpshiftLinePressure, throttlePosition, atfTempC);
  int shiftPressure = scalePressure2D(gearboxSettingsV2[gear - 2].UpshiftShiftPressure, throttlePosition, atfTempC);
  unsigned long shiftDelay = scaleDelay2D(gearboxSettingsV2[gear - 2].UpshiftDelay, throttlePosition, atfTempC);

  ledcWrite(pwmChannelsPointerV2->mpcChannel, linePressure);
  ledcWrite(pwmChannelsPointerV2->spcChannel, shiftPressure);
  digitalWrite(gearPin, HIGH);
  ledcWrite(pwmChannelsPointerV2->tccChannel, gearboxSettingsV2[gear - 2].UpshiftTorqueConverterLockup);

  activeGearPin = gearPin;
  activeFinalMpc = scalePressure2D(customMpcAfterShift, throttlePosition, atfTempC);
  activeShiftDelayMs = shiftDelay;
  activeKind = TransitionKind::Normal;
  phase = Phase::Applying;
  phaseStartMs = millis();
}

void ShiftControlV2::startSelectFivetccToFive(VaLas_Controller::GearLeverPosition currentLeverPosition, int gear, int atfTempC)
// 5 OD -> 5
{
  *screenToDisplayValueV2 = VaLas_Controller::DisplayScreen::Shifting;
  String screenVar = displayHandlerPointerV2->ToString(currentLeverPosition, gear);
  Serial.println("Downshift to " + screenVar);

  int throttlePosition = throttlePositionPointerV2 ? *throttlePositionPointerV2 : 100;
  activeShiftDelayMs = scaleDelay2D(gearboxSettingsV2[gear].DownshiftDelay, throttlePosition, atfTempC);
  activeFinalMpc = scalePressure2D(15, throttlePosition, atfTempC);
  activeKind = TransitionKind::FiveTccToFive;
  phase = Phase::Applying;
  phaseStartMs = millis();
}

void ShiftControlV2::startSelectFiveToFivetcc(VaLas_Controller::GearLeverPosition currentLeverPosition, int gear, int atfTempC)
// 5 -> 5 OD
{
  *screenToDisplayValueV2 = VaLas_Controller::DisplayScreen::Shifting;
  String screenVar = displayHandlerPointerV2->ToString(currentLeverPosition, gear);
  Serial.println("Downshift to " + screenVar);

  int throttlePosition = throttlePositionPointerV2 ? *throttlePositionPointerV2 : 100;
  activeShiftDelayMs = scaleDelay2D(gearboxSettingsV2[gear - 2].UpshiftDelay, throttlePosition, atfTempC);
  activeFinalMpc = scalePressure2D(25, throttlePosition, atfTempC);
  activeKind = TransitionKind::FiveToFiveTcc;
  phase = Phase::Applying;
  phaseStartMs = millis();
}

void ShiftControlV2::tick()
{
  unsigned long elapsed = millis() - phaseStartMs;

  if (phase == Phase::Applying)
  {
    if (elapsed < activeShiftDelayMs)
      return;

    if (activeKind == TransitionKind::ThreeToTwoDownshift)
    {
      ledcWrite(pwmChannelsPointerV2->mpcChannel, activeReducedLinePressure);
      ledcWrite(pwmChannelsPointerV2->spcChannel, activeReducedShiftPressure);
      digitalWrite(activeGearPin, LOW);
      phase = Phase::Reducing;
      phaseStartMs = millis();
      return;
    }

    finishShift();
    return;
  }

  if (phase == Phase::Reducing)
  {
    if (elapsed < 50) // Matches V1's fixed 50ms wait between the reduced-pressure step and the final MPC value
      return;

    finishShift();
  }
}

void ShiftControlV2::finishShift()
{
  switch (activeKind)
  {
    case TransitionKind::FiveToFiveTcc:
      ledcWrite(pwmChannelsPointerV2->mpcChannel, activeFinalMpc);
      ledcWrite(pwmChannelsPointerV2->spcChannel, 0);
      digitalWrite(y3Pin, LOW);
      ledcWrite(pwmChannelsPointerV2->tccChannel, (255 * 95) / 100); //95% for torque converter lockup with some slip for comfort
      break;
    case TransitionKind::FiveTccToFive:
      ledcWrite(pwmChannelsPointerV2->mpcChannel, activeFinalMpc);
      ledcWrite(pwmChannelsPointerV2->spcChannel, 0);
      digitalWrite(y3Pin, LOW);
      ledcWrite(pwmChannelsPointerV2->tccChannel, 0);
      break;
    default: // Normal and ThreeToTwoDownshift both finish the same way; the solenoid is already LOW for ThreeToTwoDownshift
      ledcWrite(pwmChannelsPointerV2->mpcChannel, activeFinalMpc);
      ledcWrite(pwmChannelsPointerV2->spcChannel, 0);
      digitalWrite(activeGearPin, LOW);
      break;
  }

  phase = Phase::Idle;
  *screenToDisplayValueV2 = VaLas_Controller::DisplayScreen::Main;
}

// Bilinear interpolation over a 3x3 grid: rows are closed/half/full throttle (fixed 0/50/100),
// columns are the cold/warm/hot ATF-temp breakpoints. Mirrors 7226ctrl's 2D map lookup, sized down to 3x3.
static int bilerp3x3(int throttlePosition, int atfTempC, const int grid[3][3], int coldT, int warmT, int hotT)
{
  const int tpsAnchors[3] = {0, 50, 100};
  int tempAnchors[3] = {coldT, warmT, hotT};

  int r0, r1;
  float rt;
  if (throttlePosition <= tpsAnchors[0]) { r0 = r1 = 0; rt = 0; }
  else if (throttlePosition >= tpsAnchors[2]) { r0 = r1 = 2; rt = 0; }
  else if (throttlePosition <= tpsAnchors[1]) { r0 = 0; r1 = 1; rt = (float)(throttlePosition - tpsAnchors[0]) / (tpsAnchors[1] - tpsAnchors[0]); }
  else { r0 = 1; r1 = 2; rt = (float)(throttlePosition - tpsAnchors[1]) / (tpsAnchors[2] - tpsAnchors[1]); }

  int c0, c1;
  float ct;
  if (tempAnchors[1] <= tempAnchors[0] || tempAnchors[2] <= tempAnchors[1]) { c0 = c1 = 1; ct = 0; } // Misconfigured breakpoints; fall back to the warm column
  else if (atfTempC <= tempAnchors[0]) { c0 = c1 = 0; ct = 0; }
  else if (atfTempC >= tempAnchors[2]) { c0 = c1 = 2; ct = 0; }
  else if (atfTempC <= tempAnchors[1]) { c0 = 0; c1 = 1; ct = (float)(atfTempC - tempAnchors[0]) / (tempAnchors[1] - tempAnchors[0]); }
  else { c0 = 1; c1 = 2; ct = (float)(atfTempC - tempAnchors[1]) / (tempAnchors[2] - tempAnchors[1]); }

  float top = grid[r0][c0] + (grid[r0][c1] - grid[r0][c0]) * ct;
  float bottom = grid[r1][c0] + (grid[r1][c1] - grid[r1][c0]) * ct;
  return (int)(top + (bottom - top) * rt);
}

int ShiftControlV2::getPressurePercent(int throttlePosition, int atfTempC)
{
  if (!pressureTimeMapPointerV2 || !pressureTimeMapPointerV2->enabled)
    return 100;

  return bilerp3x3(throttlePosition, atfTempC, pressureTimeMapPointerV2->pressurePercent,
    pressureTimeMapPointerV2->coldTempC, pressureTimeMapPointerV2->warmTempC, pressureTimeMapPointerV2->hotTempC);
}

int ShiftControlV2::getDelayPercent(int throttlePosition, int atfTempC)
{
  if (!pressureTimeMapPointerV2 || !pressureTimeMapPointerV2->enabled)
    return 100;

  return bilerp3x3(throttlePosition, atfTempC, pressureTimeMapPointerV2->delayPercent,
    pressureTimeMapPointerV2->coldTempC, pressureTimeMapPointerV2->warmTempC, pressureTimeMapPointerV2->hotTempC);
}

int ShiftControlV2::scalePressure2D(int pressure, int throttlePosition, int atfTempC)
{
  int percent = constrain(getPressurePercent(throttlePosition, atfTempC), 0, 300);
  return constrain((pressure * percent) / 100, 0, 255);
}

unsigned long ShiftControlV2::scaleDelay2D(unsigned long delayMs, int throttlePosition, int atfTempC)
{
  int percent = constrain(getDelayPercent(throttlePosition, atfTempC), 0, 300);
  return (delayMs * (unsigned long)percent) / 100;
}
