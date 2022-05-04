#ifndef SENSORS_H
#define SENSORS_H

class Sensors {
    
	public:
		Sensors();
        int ReadAtfTemp();
        int ReadRpm();
        bool init_sensors();
        bool read_input_rpm(int& n2Rpm, int& n3Rpm, int& calcRpm, bool check_sanity);
        bool read_atf_temp(int* dest);
		
	private:
};
#endif