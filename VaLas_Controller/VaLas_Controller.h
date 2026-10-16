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
#define startRelayPin 13     // Start interlock relay; HIGH when the lever is in P or N

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

// Set to an unused ADC-capable GPIO to enable the optional throttle position sensor.
// Leave disabled until the pin is selected; GPIO35 is already used by the ELR input.
#define PIN_THROTTLE_POSITION -1

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

    // TPS ADC calibration only; throttle-based pressure/delay tuning lives exclusively in PressureTimeMapSettings (ShiftControlV2).
    typedef struct
    {
      int closedAdc = 0;
      int wideOpenAdc = 4095;
    } ThrottleCalibration;

    // Bilinear 2D scaling of ShiftControlV2 pressures/delays by throttle position (rows: closed/half/full)
    // and ATF temperature (columns: cold/warm/hot breakpoints). Values are percent of the configured pressure/delay.
    typedef struct
    {
      bool enabled = true;
      int coldTempC = 0;
      int warmTempC = 60;
      int hotTempC = 110;
      int pressurePercent[3][3] = {
        {91, 70, 67},    // Closed throttle:  cold, warm, hot
        {111, 85, 81},   // Half throttle:    cold, warm, hot
        {130, 100, 95}   // Full throttle:    cold, warm, hot
      };
      int delayPercent[3][3] = {
        {182, 130, 117}, // Closed throttle:  cold, warm, hot
        {161, 115, 104}, // Half throttle:    cold, warm, hot
        {140, 100, 90}   // Full throttle:    cold, warm, hot
      };
    } PressureTimeMapSettings;

    // Trend-gated torque converter lockup for gears 3-5 (ShiftControlV2 only). Gear 5+ keeps its
    // own fixed 95% lockup, unaffected by this. Per-gear target duty is the existing
    // ShiftSetting.UpshiftTorqueConverterLockup value (index gear-1), reused as the steady-state
    // lockup target rather than a momentary shift-event value.
    typedef struct
    {
      bool enabled = false;
      int minGear = 3;                   // Lowest gear allowed to lock up (3, 4 or 5)
      int minOutputShaftRpm = 1200;       // Don't engage below this gear-ratio-normalized output-shaft RPM (not calculatedRpm directly - see gearRatios)
      int maxThrottleForEngage = 55;      // Don't engage above this throttle % (avoid locking under load)
      int decelRpmPerSecThreshold = 150;  // RPM/sec drop that blocks/releases lockup (coast/braking)
      int engageDelayMs = 800;            // Steady-state dwell required before engaging
      // Typical 722.6 ratios (1st..5th); confirm/adjust for the exact variant fitted. Used only to
      // 722.6/5G-Tronic 1st-5th ratios (pre-2004; see en.wikipedia.org/wiki/Mercedes-Benz_5G-Tronic_transmission).
      // Default here is the W5A580 "big case" (580Nm+) set, matching this controller's own box;
      // W5A330 "small case" (<=330Nm) is 3.9319/2.4079/1.4857/1.0000/0.8305. Used only to normalize
      // minOutputShaftRpm across gears - calculatedRpm (turbine RPM) / gearRatios[gear-1].
      float gearRatios[5] = {3.5876f, 2.1862f, 1.4054f, 1.0000f, 0.8314f};
    } LockupControlSettings;
		
	private:
};

#endif
