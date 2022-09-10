#ifndef TASKSTRUCTS_H
#define TASKSTRUCTS_H

#include "VaLas_Controller.h"
#include "Gearlever.h"

class TaskStructs {
    
	public:
		struct gearLeverParameters
        {
            VaLas_Controller::ShiftRequest* currentShiftRequestPtr;
            VaLas_Controller::GearLeverPosition* currentLeverPositionPtr;
            VaLas_Controller::GearLeverPosition* oldLeverPositionPtr;
        };
        typedef struct gearLeverParameters GearLeverParameters;

        struct shiftControlParameters
        {
            int* gearPtr;
            VaLas_Controller::GearLeverPosition* currentLeverPositionPtr;
            VaLas_Controller::GearLeverPosition* oldLeverPositionPtr;
            VaLas_Controller::ShiftRequest* currentShiftRequestPtr;
            VaLas_Controller::ShiftSetting* shiftSettings;
        };
        typedef struct shiftControlParameters ShiftControlParameters;

        struct shiftConfigParameters
        {
            bool* useCanBusPtr;
            VaLas_Controller::ShiftSetting* shiftSettings;
        };
        typedef struct shiftConfigParameters ShiftConfigParameters;

        struct displayHandlerParameters
        {
            VaLas_Controller::DisplayScreen* screenToDisplayPtr;
            int* currentGearPtr;
            VaLas_Controller::GearLeverPosition* currentLeverPositionPtr;
            VaLas_Controller::ShiftRequest* currentShiftRequestPtr;
            int* atfTempPtr;
        };
        typedef struct displayHandlerParameters DisplayHandlerParameters;
		
	private:

};
#endif
