#include "VaLas_Controller.h"
#include "DisplayHandler.h"

#ifndef SHIFTCONTROLV2_H
#define SHIFTCONTROLV2_H

// Non-blocking counterpart to ShiftControl: same pressures/delays, but phased via millis() instead of vTaskDelay,
// plus a 2D throttle-position x ATF-temperature scaling of pressure and shift duration.
class ShiftControlV2 {

	public:
		void init(DisplayHandler* displayHandlerPtr, VaLas_Controller::PwmChannels* pwmChannelsPtr, Gearlever* gearLeverPtr,
			VaLas_Controller::DisplayScreen* screenToDisplayPtr, VaLas_Controller::ShiftSetting* gearboxSettingsPtr);
		void execute(void * parameter);

	private:
		enum class Phase { Idle, Applying, Reducing };
		// Distinguishes the final-state write and whether a Reducing phase runs in between.
		enum class TransitionKind { Normal, ThreeToTwoDownshift, FiveToFiveTcc, FiveTccToFive };

		Phase phase = Phase::Idle;
		TransitionKind activeKind = TransitionKind::Normal;
		unsigned long phaseStartMs = 0;
		unsigned long activeShiftDelayMs = 0;
		int activeGearPin = -1;
		int activeFinalMpc = 0;
		int activeReducedLinePressure = 0;
		int activeReducedShiftPressure = 0;

		void processLeverValues(VaLas_Controller::GearLeverPosition oldLeverPosition, VaLas_Controller::GearLeverPosition currentLeverPosition, int* gear);
		void resetToGear2(VaLas_Controller::GearLeverPosition currentLeverPosition, int* gear);
		void startDownShift(int customMpcAfterShift, VaLas_Controller::GearLeverPosition currentLeverPosition, int gear, int atfTempC);
		void startUpShift(int customMpcAfterShift, VaLas_Controller::GearLeverPosition currentLeverPosition, int gear, int atfTempC);
		void startSelectFivetccToFive(VaLas_Controller::GearLeverPosition currentLeverPosition, int gear, int atfTempC);
		void startSelectFiveToFivetcc(VaLas_Controller::GearLeverPosition currentLeverPosition, int gear, int atfTempC);
		void tick();
		void finishShift();
		int getEffectiveThrottlePosition();
		int getPressurePercent(int throttlePosition, int atfTempC);
		int getDelayPercent(int throttlePosition, int atfTempC);
		int scalePressure2D(int pressure, int throttlePosition, int atfTempC);
		unsigned long scaleDelay2D(unsigned long delayMs, int throttlePosition, int atfTempC);
};
#endif
