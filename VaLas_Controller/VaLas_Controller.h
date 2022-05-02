#ifndef VALAS_CONTROLLER_H
#define VALAS_CONTROLLER_H


// GEAR SETTINGS
// PIN  1-2,4-5 SWITCH  ON/OFF
// PIN2 2-3     SWITCH  ON/OFF
// PIN3 3-4     SWITCH  ON/OFF
// PIN4 LINE PRESSURE   PWM 0-255 low=0
// PIN5 SWITCH PRESSURE PWM 0-255 low=0
// PIN6 TURBINE LOCK    PWM 0-255 low=0

#define upShiftPin 19
#define downShiftPin 18
#define gearLeverPotPin 4    // Read potentiometer value to determine if in P, R, N, D

#define y3Pin 33             // 1-2, 4-5 switch    shift      LOW/HIGH
#define y4Pin 35             // 3-4 switch         shift      LOW/HIGH
#define y5Pin 37             // 2-3 switch         shift      LOW/HIGH
#define mpcPin 39            // Line pressure      MOD_PC     min-max 255-0
#define spcPin 41            // Shift pressure     SHIFT_PC   min-max 255-0
#define tccPin 43            // Turbine lockup     TCC        min-max 0-255
#define atfTempPin 40        // ATF temp / P-N switch 

#define elrTogglePin 100
#define elrPwmPin 101
#define elrPwmFreq 100
#define elrChannel 20

class VaLas_Controller {
    
	public:
    enum class GearLeverPosition
    {
      Park,
      Reverse,
      Neutral,
      Drive,
      Unknown
    };

    enum class ShiftRequest
    {
      NoShift,
      UpShift,
      DownShift
    };

    typedef struct
    {
      String Name;
      int UpshiftDelay = 600;
      int UpshiftLinePressure = 0;                  //  MOD_PC     min-max 255-0
      int UpshiftShiftPressure = 0;                 //  SHIFT_PC   min-max 255-0
      int UpshiftTorqueConverterLockup = 0;         //  TCC        min-max 0-255
      int DownshiftDelay = 600;
      int DownshiftLinePressure = 0;                //  MOD_PC     min-max 255-0
      int DownshiftShiftPressure = 0;               //  SHIFT_PC   min-max 255-0
      int DownshiftTorqueConverterLockup = 0;       //  TCC        min-max 0-255
    }  ShiftSetting;
		
	private:
};

#endif
