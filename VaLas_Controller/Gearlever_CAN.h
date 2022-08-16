#include "VaLas_Controller.h"
#include "Gearlever.h"

#ifndef GEARLEVER_CAN_H
#define GEARLEVER_CAN_H

class Gearlever_CAN : public Gearlever {
    
	public:
		Gearlever_CAN();
        virtual void ReadGearLever(void * parameter);
		virtual void Reset();
		
	private:
		void readCanBus();
};
#endif
