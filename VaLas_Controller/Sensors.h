#ifndef SENSORS_H
#define SENSORS_H

#include "VaLas_Controller.h"

class Sensors {
    
	public:
		Sensors();
        int ReadAtfTemp();
        int ReadRpm();  // Returns transmission RPM (from N2/N3 sensors)
        int ReadEngineRpm(); // Returns engine RPM (from crankshaft sensor)
        void SetEngineConfig(VaLas_Controller::EngineType engineType, VaLas_Controller::RpmGaugeType gaugeType);
        void OutputRpmToGauge(int engineRpm); // Outputs correct signal to RPM gauge
        bool init_sensors();
        bool read_input_rpm(int& n2Rpm, int& n3Rpm, int& calcRpm, bool check_sanity);
        bool read_atf_temp(int* dest);
        bool read_engine_rpm(int* engineRpm); // Raw engine RPM reading
		
	private:
        VaLas_Controller::EngineType currentEngineType = VaLas_Controller::EngineType::OM606;
        VaLas_Controller::RpmGaugeType currentGaugeType = VaLas_Controller::RpmGaugeType::Mercedes300E;
        volatile uint32_t enginePulseCount = 0;
        uint32_t lastEngineReadTime = 0;
        void setupEngineRpmPCNT();
        int calculateEngineRpm(uint32_t pulseCount, uint32_t timeMs);
        int calculateGaugeOutput(int engineRpm);
};
#endif