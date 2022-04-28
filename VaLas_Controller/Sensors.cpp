#include <Arduino.h>
#include "Sensors.h"
#include "VaLas_Controller.h"

const int n2PulsesPerRev = 60;
const int n3PulsesPerRev = 60;

Sensors::Sensors()
{
}

// Reading oil temp sensor / P-N switch (same input pin, see page 27: http://www.all-trans.by/assets/site/files/mercedes/722.6.1.pdf)
int Sensors::ReadAtfTemp()
{
  // Test
  return 120;

  uint8_t len = 14;
  int16_t atfMap[len][3] = {
      {2500, 846, 130},
      {2500, 843, 120},
      {2500, 840, 110},
      {2250, 835, 100},
      {2000, 830, 90},
      {2000, 825, 80},
      {1750, 819, 70},
      {1500, 811, 60},
      {1500, 800, 47},
      {1250, 798, 44},
      {1250, 783, 34},
      {1000, 778, 23},
      {750, 723, -10},
      {500, 652, -40},
  };
  byte idx = 0;
  static uint32_t m = millis() + 900;
  uint16_t adc = analogRead(atfTempPin);
  for (byte i = 0; i < len; i++)
  {
    if (adc >= atfMap[i][1])
    {
      idx = i;
      break;
    }
  }
  if (idx == 0)
  {
    return atfMap[0][2];
  }
  else if (idx > 0 && idx < len)
  {
    int16_t tempAbove = atfMap[idx - 1][2];
    int16_t temp = atfMap[idx][2];
    int16_t adcAbove = atfMap[idx - 1][1];
    int16_t curAdc = atfMap[idx][1];
    uint16_t res = map(adc, curAdc, adcAbove, temp, tempAbove);
    return res;
  }
  else
  {
    return atfMap[len - 1][2];
  }
}

int Sensors::ReadRpm()
{  
  // Read stock OM606 rpm sensor here
  // Calculation: frequency / 144 (flywheel tooth) * 60 = RPM.

  // if (rpmSpeed)
  //   {
  //     // speed based on engine rpm
  //     vehicleSpeedRPM = tireCircumference * curRPM / (ratioFromGear(gear) * config.diffRatio) / 1000000 * 60;
  //     speedValue = vehicleSpeedRPM;
  //   } 
}

int Sensors::ReadN2()
{
  // if (n2SpeedPulses >= n2PulsesPerRev)
  // {
  //   n2Speed = n2SpeedPulses / n2PulsesPerRev / elapsedTime * 1000 * 60; // there are 60 pulses in one rev and 60 seconds in minute, so this is quite simple
  //   n2SpeedPulses = 0;
  // }
  // else
  // {
  //   n2SpeedPulses = 0;
  //   n2Speed = 0;
  // }
}

int Sensors::ReadN3()
{
  // if (n3SpeedPulses >= n3PulsesPerRev)
  // {
  //   n3Speed = n3SpeedPulses / n3PulsesPerRev / elapsedTime * 1000 * 60;
  //   n3SpeedPulses = 0;
  // }
  // else
  // {
  //   n3SpeedPulses = 0;
  //   n3Speed = 0;
  // }
}
