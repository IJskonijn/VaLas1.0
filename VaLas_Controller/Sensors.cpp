#include <Arduino.h>
#include "Sensors.h"
#include "VaLas_Controller.h"
#include "driver/pcnt.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver/timer.h"
#include "esp_log.h"

static const char* LOG_TAG = "SENSORS";

const int n2PulsesPerRev = 60;
const int n3PulsesPerRev = 60;

#define PULSES_PER_REV 120 // N2 and N3 are 60 pulses per revolution
#define PCNT_H_LIM PULSES_PER_REV * 10
#define ADC_CHANNEL_VBATT adc2_channel_t::ADC2_CHANNEL_8
#define ADC_CHANNEL_ATF adc2_channel_t::ADC2_CHANNEL_9
#define ADC2_ATTEN ADC_ATTEN_11db
#define ADC2_WIDTH ADC_WIDTH_12Bit
#define TIMER_INTERVAL_MS 20 // Every 20ms we poll RPM (Same as other ECUs)
#define PULSE_MULTIPLIER 1000/TIMER_INTERVAL_MS

portMUX_TYPE n2_mux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE n3_mux = portMUX_INITIALIZER_UNLOCKED;

uint64_t n2_overflow = 0;
uint64_t n3_overflow = 0;

volatile uint64_t n2_rpm = 0;
volatile uint64_t n3_rpm = 0;

const pcnt_unit_t PCNT_N2_RPM = PCNT_UNIT_0;
const pcnt_unit_t PCNT_N3_RPM = PCNT_UNIT_1;
const pcnt_unit_t PCNT_ENGINE_RPM = PCNT_UNIT_2;

// Engine RPM variables
portMUX_TYPE engine_mux = portMUX_INITIALIZER_UNLOCKED;
volatile uint64_t engine_pulse_count = 0;
volatile uint64_t engine_overflow = 0;

Sensors::Sensors()
{
}

// Reading oil temp sensor / P-N switch (same input pin, see page 27: http://www.all-trans.by/assets/site/files/mercedes/722.6.1.pdf)
int Sensors::ReadAtfTemp()
{
  // Test
  return 120;

  // uint8_t len = 14;
  // int16_t atfMap[len][3] = {
  //     {2500, 846, 130},
  //     {2500, 843, 120},
  //     {2500, 840, 110},
  //     {2250, 835, 100},
  //     {2000, 830, 90},
  //     {2000, 825, 80},
  //     {1750, 819, 70},
  //     {1500, 811, 60},
  //     {1500, 800, 47},
  //     {1250, 798, 44},
  //     {1250, 783, 34},
  //     {1000, 778, 23},
  //     {750, 723, -10},
  //     {500, 652, -40},
  // };
  // byte idx = 0;
  // static uint32_t m = millis() + 900;
  // uint16_t adc = analogRead(atfTempPin);
  // for (byte i = 0; i < len; i++)
  // {
  //   if (adc >= atfMap[i][1])
  //   {
  //     idx = i;
  //     break;
  //   }
  // }
  // if (idx == 0)
  // {
  //   return atfMap[0][2];
  // }
  // else if (idx > 0 && idx < len)
  // {
  //   int16_t tempAbove = atfMap[idx - 1][2];
  //   int16_t temp = atfMap[idx][2];
  //   int16_t adcAbove = atfMap[idx - 1][1];
  //   int16_t curAdc = atfMap[idx][1];
  //   uint16_t res = map(adc, curAdc, adcAbove, temp, tempAbove);
  //   return res;
  // }
  // else
  // {
  //   return atfMap[len - 1][2];
  // }
}

int Sensors::ReadRpm()
{  
  // This function returns transmission RPM calculated from N2/N3 sensors
  int n2Rpm, n3Rpm, calculatedRpm;
  
  if (read_input_rpm(n2Rpm, n3Rpm, calculatedRpm, false)) {
    return calculatedRpm;
  } else {
    return 0; // Return 0 if transmission RPM reading fails
  }
}

typedef struct {
    uint16_t v;  // Voltage in mV
    int temp;    // ATF Temp in degrees C * 10
} temp_reading_t;

#define NUM_TEMP_POINTS 22
const static temp_reading_t atf_temp_lookup[NUM_TEMP_POINTS] = {
//    mV, Temp(x10)
// mV Values are calibrated on 3.45V rail
// as that is how much the ATF sensor power gets
    {446, -400},
    {461, -300},
    {476, -200},
    {491, -100},
    {507, 0},
    {523, 100},
    {540, 200},
    {557, 300},
    {574, 400},
    {592, 500},
    {610, 600},
    {629, 700},
    {648, 800},
    {669, 900},
    {690, 1000},
    {711, 1100},
    {732, 1200},
    {755, 1300},
    {778, 1400},
    {802, 1500},
    {814, 1600},
    {851, 1700}
};

bool Sensors::read_input_rpm(int& n2Rpm, int& n3Rpm, int& calcRpm, bool check_sanity) {
    n2Rpm = ((float)n2_rpm * (float)PULSE_MULTIPLIER * 0.5); // *0.5 as counting both Inc and Dec pulse dir
    n3Rpm = ((float)n3_rpm * (float)PULSE_MULTIPLIER * 0.5); // *0.5 as counting both Inc and Dec pulse dir
    //ESP_LOGI("RPM", "N2 %d, N3 %d", n2Rpm, n3Rpm);
    if (n2Rpm < 10 && n3Rpm < 10) { // Stationary, break here to avoid divideBy0Ex
        calcRpm = 0;
        return true;
    } else if (n2Rpm == 0) { // In gears R1 or R2 (as N2 is 0)
        calcRpm = n3Rpm;
        return true;
    } else {
        if (abs((int)n2Rpm - (int)n3Rpm) < 50) {
            calcRpm = (n2Rpm+n3Rpm)/2;
            return true;
        }
        // More difficult calculation for all forward gears
        // This calculation works when both RPM sensors are the same (Gears 2,3,4)
        // Or when N3 is 0 and N2 is reporting ~0.61x real Rpm (Gears 1 and 5)
        // Also nicely handles transitionary phases between RPM readings, making gear shift RPM readings
        // a lot more accurate for the rest of the TCM code
        
        float ratio = (float)n3Rpm/(float)n2Rpm;
        float f2 = (float)n2Rpm;
        float f3 = (float)n3Rpm;

        calcRpm = ((f2*1.64f)*(1.0f-ratio))+(f3*ratio);

        // If we need to check sanity, check it, in gears 2,3 and 4, RPM readings should be the same,
        // otherwise we have a faulty conductor place sensor!
        return check_sanity ? abs((int)n2Rpm - (int)n3Rpm) < 150 : true;
    }
}

// Returns ATF temp in *C
bool Sensors::read_atf_temp(int* dest){
    #define NUM_ATF_SAMPLES 5
    uint32_t raw = 0;
    uint32_t avg = 0;
    for (uint8_t i = 0; i < NUM_ATF_SAMPLES; i++) {
        raw = analogRead(PIN_ATF); // Read the actual ADC value
        if (raw >= 1000) {
            return false; // Parking lock engaged, cannot read.
        }
        avg += raw;
    }
    avg /= NUM_ATF_SAMPLES;
    
    // Convert ADC reading to millivolts (assuming 3.3V reference and 12-bit ADC)
    uint32_t voltage_mv = (avg * 3300) / 4095;
    
    //ESP_LOGI("ATF", "AVG VOLTAGE %d mV", voltage_mv);
    if (voltage_mv < atf_temp_lookup[0].v) {
        *dest = atf_temp_lookup[0].temp / 10;
        return true;
    } else if (voltage_mv > atf_temp_lookup[NUM_TEMP_POINTS-1].v) {
        *dest = (atf_temp_lookup[NUM_TEMP_POINTS-1].temp) / 10;
        return true;
    } else {
        for (uint8_t i = 0; i < NUM_TEMP_POINTS-1; i++) {
            // Found! Interpolate linearly to get a better estimate of ATF Temp
            if (atf_temp_lookup[i].v <= voltage_mv && atf_temp_lookup[i+1].v >= voltage_mv) {
                float dx = voltage_mv - atf_temp_lookup[i].v;
                float dy = atf_temp_lookup[i+1].v - atf_temp_lookup[i].v;
                *dest = (atf_temp_lookup[i].temp + (atf_temp_lookup[i+1].temp-atf_temp_lookup[i].temp) * ((dx)/dy)) / 10;
                return true;
            }
        }
        return true;
    }
}

// Engine RPM Configuration and Reading Functions

void Sensors::SetEngineConfig(VaLas_Controller::EngineType engineType, VaLas_Controller::RpmGaugeType gaugeType) {
    currentEngineType = engineType;
    currentGaugeType = gaugeType;
    ESP_LOGI(LOG_TAG, "Engine config set: %s, Gauge: %s", 
             engineType == VaLas_Controller::EngineType::OM603 ? "OM603" : "OM606",
             gaugeType == VaLas_Controller::RpmGaugeType::Mercedes300D ? "300D" : "300E");
}

int Sensors::ReadEngineRpm() {
    int engineRpm = 0;
    if (read_engine_rpm(&engineRpm)) {
        return engineRpm;
    }
    return 0;
}

bool Sensors::read_engine_rpm(int* engineRpm) {
    // Read current pulse count from PCNT
    int16_t pulseCount = 0;
    uint32_t currentTime = millis();
    
    portENTER_CRITICAL(&engine_mux);
    pcnt_get_counter_value(PCNT_ENGINE_RPM, &pulseCount);
    pcnt_counter_clear(PCNT_ENGINE_RPM);
    pulseCount += (engine_overflow * PCNT_H_LIM);
    engine_overflow = 0;
    portEXIT_CRITICAL(&engine_mux);
    
    if (lastEngineReadTime == 0) {
        lastEngineReadTime = currentTime;
        *engineRpm = 0;
        return true;
    }
    
    uint32_t deltaTime = currentTime - lastEngineReadTime;
    lastEngineReadTime = currentTime;
    
    if (deltaTime == 0) {
        *engineRpm = 0;
        return true;
    }
    
    *engineRpm = calculateEngineRpm(pulseCount, deltaTime);
    return true;
}

int Sensors::calculateEngineRpm(uint32_t pulseCount, uint32_t timeMs) {
    if (timeMs == 0 || pulseCount == 0) {
        return 0;
    }
    
    // Calculate RPM based on engine type
    float pulsesPerRevolution;
    
    switch (currentEngineType) {
        case VaLas_Controller::EngineType::OM603:
            pulsesPerRevolution = 144.0f; // 144 flywheel teeth
            break;
        case VaLas_Controller::EngineType::OM606:
            pulsesPerRevolution = 6.0f;   // 6 flywheel tabs
            break;
        default:
            pulsesPerRevolution = 6.0f;
            break;
    }
    
    // RPM = (pulses / pulsesPerRevolution) / (timeMs / 60000)
    float revolutions = (float)pulseCount / pulsesPerRevolution;
    float timeMinutes = (float)timeMs / 60000.0f;
    
    if (timeMinutes > 0) {
        return (int)(revolutions / timeMinutes);
    }
    
    return 0;
}

void Sensors::OutputRpmToGauge(int engineRpm) {
    int gaugePulses = calculateGaugeOutput(engineRpm);
    
    // Output PWM signal for the gauge
    // Frequency represents the RPM signal
    if (gaugePulses > 0) {
        // Set PWM frequency based on required pulses per second
        int frequency = gaugePulses;
        if (frequency > 10000) frequency = 10000; // Cap at 10kHz
        if (frequency < 1) frequency = 1;         // Minimum 1Hz
        
        ledcWriteTone(RPM_GAUGE_CHANNEL, frequency);
        ledcWrite(RPM_GAUGE_CHANNEL, 128); // 50% duty cycle
    } else {
        ledcWrite(RPM_GAUGE_CHANNEL, 0); // No signal when RPM is 0
    }
}

int Sensors::calculateGaugeOutput(int engineRpm) {
    if (engineRpm <= 0) {
        return 0;
    }
    
    int pulsesPerRpm;
    
    switch (currentGaugeType) {
        case VaLas_Controller::RpmGaugeType::Mercedes300E:
            pulsesPerRpm = 3;   // 300E gauge needs 3 pulses per RPM
            break;
        case VaLas_Controller::RpmGaugeType::Mercedes300D:
            pulsesPerRpm = 144; // 300D gauge needs 144 pulses per RPM
            break;
        default:
            pulsesPerRpm = 3;
            break;
    }
    
    // Calculate pulses per second needed for the gauge
    // For a gauge showing RPM, we need pulsesPerRpm * (engineRpm / 60) pulses per second
    return (pulsesPerRpm * engineRpm) / 60;
}
