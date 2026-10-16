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
            int* throttlePositionPtr;
            bool* useThrottlePositionPtr;
            VaLas_Controller::ThrottleCalibration* throttleSettingsPtr;
            int* atfTempPtr;
            VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr;
            int* calculatedRpmPtr;
            int* rpmTrendPtr;
            VaLas_Controller::LockupControlSettings* lockupSettingsPtr;
        };
        typedef struct shiftControlParameters ShiftControlParameters;

        struct shiftConfigParameters
        {
            bool* useCanBusPtr;
            bool* usePedalShiftersPtr;
            bool* useLargeDisplayPtr;
            bool* useThrottlePositionPtr;
            VaLas_Controller::ShiftSetting* shiftSettings;
            VaLas_Controller::ThrottleCalibration* throttleSettingsPtr;
            VaLas_Controller::PressureTimeMapSettings* pressureTimeMapPtr;
            VaLas_Controller::LockupControlSettings* lockupSettingsPtr;
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

        struct sensorParameters
        {
            int* atfTempPtr;
            int* n2RpmPtr;
            int* n3RpmPtr;
            int* calculatedRpmPtr;
            int* rpmTrendPtr;
            int* throttlePositionPtr;
            bool* useThrottlePositionPtr;
            VaLas_Controller::ThrottleCalibration* throttleSettingsPtr;
        };
        typedef struct sensorParameters SensorParameters;
		
	private:

};
#endif
