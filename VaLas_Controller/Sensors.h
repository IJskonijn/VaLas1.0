#ifndef SENSORS_H
#define SENSORS_H

class Sensors {
    
	public:
		Sensors();
        int ReadAtfTemp();
        int ReadRpm();
        int ReadN2();
        int ReadN3();
		
	private:
};
#endif
