#ifndef VALAS_CONTROLLER_H
#define VALAS_CONTROLLER_H

// GEAR SETTINGS
// PIN  1-2,4-5 SWITCH  ON/OFF
// PIN2 2-3     SWITCH  ON/OFF
// PIN3 3-4     SWITCH  ON/OFF
// PIN4 LINE PRESSURE   PWM 0-255 low=0
// PIN5 SWITCH PRESSURE PWM 0-255 low=0
// PIN6 TURBINE LOCK    PWM 0-255 low=0

// https://lastminuteengineers.com/esp32-pinout-reference/

/// Canbus/SPI pins
#define canInt 2

#define spiClk 18
#define spiMiso 19
#define spiMosi 23
#define spiCso 5

#define upShiftPin 16
#define downShiftPin 17
#define gearLeverPotPin 4    // Read potentiometer value to determine if in P, R, N, D

#define y3Pin 32             // 1-2, 4-5 switch    shift      LOW/HIGH
#define y4Pin 33             // 3-4 switch         shift      LOW/HIGH
#define y5Pin 25             // 2-3 switch         shift      LOW/HIGH
#define mpcPin 26            // Line pressure      MOD_PC     min-max 255-0
#define spcPin 27            // Shift pressure     SHIFT_PC   min-max 255-0
#define tccPin 14            // Turbine lockup     TCC        min-max 0-255

// #define atfTempPin 36        // ATF temp / P-N switch 
// #define n2Pin 39        // ATF temp / P-N switch 
// #define n3Pin 34        // ATF temp / P-N switch 
#define PIN_ATF gpio_num_t::GPIO_NUM_36 // ATF temp sensor and lockout
#define PIN_N2 gpio_num_t::GPIO_NUM_39 // N2 speed sensor
#define PIN_N3 gpio_num_t::GPIO_NUM_34 // N3 speed sensor

// Engine RPM sensor and gauge output pins
#define PIN_ENGINE_RPM gpio_num_t::GPIO_NUM_15 // Engine RPM sensor input
#define PIN_RPM_GAUGE_OUT 2                    // RPM gauge output signal
#define RPM_GAUGE_CHANNEL 4                    // PWM channel for RPM gauge

#define elrTogglePin 35
#define elrPwmPin 5
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

    enum class DisplayScreen
    {
      Main,
      Shifting
    };

    enum class EngineType
    {
      OM603,  // 144 flywheel teeth
      OM606   // 6 flywheel tabs
    };

    enum class RpmGaugeType
    {
      Mercedes300E,  // Needs 3 pulses per RPM
      Mercedes300D   // Needs 144 pulses per RPM (same as OM603 flywheel)
    };

    typedef struct
    {
      int mpcChannel = 0; // Channel 0
      int spcChannel = 1; // Channel 1
      int tccChannel = 2; // Channel 2
      int y4Channel = 3;  // Channel 3
    } PwmChannels;

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
