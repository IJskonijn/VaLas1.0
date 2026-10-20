#ifndef SENSORS_H
#define SENSORS_H

#include "VaLas_Controller.h"

class Sensors {
    
	public:
        Sensors();
        int ReadRpm();  // Returns transmission RPM (from N2/N3 sensors)
        int ReadEngineRpm(); // Returns engine RPM (from crankshaft sensor)
        int ReadThrottlePosition(); // Returns optional throttle position as 0-100 percent
        int ReadThrottleAdc(); // Returns averaged raw TPS ADC value, or -1 when unavailable
        void SetEngineConfig(VaLas_Controller::EngineType engineType, VaLas_Controller::RpmGaugeType gaugeType);
        void OutputRpmToGauge(int engineRpm); // Outputs correct signal to RPM gauge
        void setupInputSpeedPcnt(); // Configures the N2/N3 PCNT units; call once from setup()
        bool read_input_rpm(int& n2Rpm, int& n3Rpm, int& calcRpm, bool check_sanity);
        int GetRpmTrendPerSec(); // Signed calculatedRpm rate of change from the last read_input_rpm() call
        int GetLastN2Pulses(); // Raw N2 pulse count from the last read_input_rpm() call (ground truth for wiring checks)
        int GetLastN3Pulses(); // Raw N3 pulse count from the last read_input_rpm() call
        int GetLastN2Rpm(); // n2Rpm from the last read_input_rpm() call
        int GetLastN3Rpm(); // n3Rpm from the last read_input_rpm() call
        int GetLastCalculatedRpm(); // calcRpm from the last read_input_rpm() call (-1 if never read)
        bool read_atf_temp(int* dest);
        int GetLastAtfTempC(); // ATF temp (*C) from the last successful read_atf_temp() call
        bool IsLastAtfValid(); // false while the reading last failed (e.g. P/N park-lock quirk)
        bool read_throttle_position(int* dest, const VaLas_Controller::ThrottleCalibration& settings); // Reads and calibrates optional throttle position
        bool read_engine_rpm(int* engineRpm); // Raw engine RPM reading
		
	private:
        VaLas_Controller::EngineType currentEngineType = VaLas_Controller::EngineType::OM606;
        VaLas_Controller::RpmGaugeType currentGaugeType = VaLas_Controller::RpmGaugeType::Mercedes300E;
        volatile uint32_t enginePulseCount = 0;
        uint32_t lastEngineReadTime = 0;
        uint32_t lastSpeedReadTime = 0;
        int lastCalcRpm = -1;
        int rpmTrendPerSec = 0;
        int lastN2Pulses = 0;
        int lastN3Pulses = 0;
        int lastN2Rpm = 0;
        int lastN3Rpm = 0;
        int lastAtfTempC = 0;
        bool lastAtfValid = false;
        void setupEngineRpmPCNT();
        int calculateEngineRpm(uint32_t pulseCount, uint32_t timeMs);
        int calculateGaugeOutput(int engineRpm);
};
#endif
