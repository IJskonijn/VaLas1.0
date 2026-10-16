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

portMUX_TYPE n2_mux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE n3_mux = portMUX_INITIALIZER_UNLOCKED;

const pcnt_unit_t PCNT_N2_RPM = PCNT_UNIT_0;
const pcnt_unit_t PCNT_N3_RPM = PCNT_UNIT_1;
const pcnt_unit_t PCNT_ENGINE_RPM = PCNT_UNIT_2;

// Max int16 so a pulse burst can never silently wrap the hardware counter between our ~100ms reads
// (the previous PCNT_H_LIM=1200 was sized for an unimplemented 20ms ISR-driven clear cycle).
#define PCNT_SPEED_H_LIM 32767

// Engine RPM variables
portMUX_TYPE engine_mux = portMUX_INITIALIZER_UNLOCKED;
volatile uint64_t engine_pulse_count = 0;
volatile uint64_t engine_overflow = 0;

Sensors::Sensors()
{
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

int Sensors::ReadThrottlePosition()
{
    int throttlePosition = 0;
    VaLas_Controller::ThrottleCalibration settings;
    if (read_throttle_position(&throttlePosition, settings)) {
        return throttlePosition;
    }
    return -1;
}

int Sensors::ReadThrottleAdc()
{
#if PIN_THROTTLE_POSITION < 0
    return -1;
#else
    const uint8_t sampleCount = 5;
    uint32_t average = 0;

    for (uint8_t i = 0; i < sampleCount; i++) {
        average += analogRead(PIN_THROTTLE_POSITION);
    }
    return average / sampleCount;
#endif
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

void Sensors::setupInputSpeedPcnt() {
    pcnt_config_t n2Config = {};
    n2Config.pulse_gpio_num = PIN_N2;
    n2Config.ctrl_gpio_num = PCNT_PIN_NOT_USED;
    n2Config.channel = PCNT_CHANNEL_0;
    n2Config.unit = PCNT_N2_RPM;
    n2Config.pos_mode = PCNT_COUNT_INC; // Count both edges for 2x resolution (halved back out in read_input_rpm)
    n2Config.neg_mode = PCNT_COUNT_INC;
    n2Config.lctrl_mode = PCNT_MODE_KEEP;
    n2Config.hctrl_mode = PCNT_MODE_KEEP;
    n2Config.counter_h_lim = PCNT_SPEED_H_LIM;
    n2Config.counter_l_lim = 0;

    pcnt_config_t n3Config = n2Config;
    n3Config.pulse_gpio_num = PIN_N3;
    n3Config.unit = PCNT_N3_RPM;

    // GPIO34-39 are input-only with no internal pull resistor; the sensor harness supplies its own bias.
    pinMode(PIN_N2, INPUT);
    pinMode(PIN_N3, INPUT);

    pcnt_unit_config(&n2Config);
    pcnt_set_filter_value(PCNT_N2_RPM, 200); // ~2.5us glitch filter at the 80MHz APB clock
    pcnt_filter_enable(PCNT_N2_RPM);
    pcnt_counter_pause(PCNT_N2_RPM);
    pcnt_counter_clear(PCNT_N2_RPM);
    pcnt_counter_resume(PCNT_N2_RPM);

    pcnt_unit_config(&n3Config);
    pcnt_set_filter_value(PCNT_N3_RPM, 200);
    pcnt_filter_enable(PCNT_N3_RPM);
    pcnt_counter_pause(PCNT_N3_RPM);
    pcnt_counter_clear(PCNT_N3_RPM);
    pcnt_counter_resume(PCNT_N3_RPM);

    lastSpeedReadTime = 0;
    lastCalcRpm = -1;
    rpmTrendPerSec = 0;
}

bool Sensors::read_input_rpm(int& n2Rpm, int& n3Rpm, int& calcRpm, bool check_sanity) {
    int16_t n2Pulses = 0, n3Pulses = 0;
    uint32_t currentTime = millis();

    portENTER_CRITICAL(&n2_mux);
    pcnt_get_counter_value(PCNT_N2_RPM, &n2Pulses);
    pcnt_counter_clear(PCNT_N2_RPM);
    portEXIT_CRITICAL(&n2_mux);

    portENTER_CRITICAL(&n3_mux);
    pcnt_get_counter_value(PCNT_N3_RPM, &n3Pulses);
    pcnt_counter_clear(PCNT_N3_RPM);
    portEXIT_CRITICAL(&n3_mux);

    if (lastSpeedReadTime == 0) {
        lastSpeedReadTime = currentTime;
        n2Rpm = n3Rpm = calcRpm = 0;
        return true;
    }

    uint32_t deltaMs = currentTime - lastSpeedReadTime;
    lastSpeedReadTime = currentTime;
    if (deltaMs == 0) {
        n2Rpm = n3Rpm = calcRpm = 0;
        return true;
    }

    // Each physical pulse is counted twice (rising+falling edge), so halve before converting to RPM.
    n2Rpm = (int)(((float)n2Pulses * 0.5f * 60000.0f) / ((float)n2PulsesPerRev * (float)deltaMs));
    n3Rpm = (int)(((float)n3Pulses * 0.5f * 60000.0f) / ((float)n3PulsesPerRev * (float)deltaMs));

    bool sane = true;
    if (n2Rpm < 10 && n3Rpm < 10) { // Stationary, avoid divideBy0Ex
        calcRpm = 0;
    } else if (n2Rpm == 0) { // In gears R1 or R2 (as N2 is 0)
        calcRpm = n3Rpm;
    } else if (abs(n2Rpm - n3Rpm) < 50) {
        calcRpm = (n2Rpm + n3Rpm) / 2;
    } else {
        // More difficult calculation for all forward gears
        // This calculation works when both RPM sensors are the same (Gears 2,3,4)
        // Or when N3 is 0 and N2 is reporting ~0.61x real Rpm (Gears 1 and 5)
        // Also nicely handles transitionary phases between RPM readings, making gear shift RPM readings
        // a lot more accurate for the rest of the TCM code
        float ratio = (float)n3Rpm / (float)n2Rpm;
        float f2 = (float)n2Rpm;
        float f3 = (float)n3Rpm;

        calcRpm = (int)(((f2 * 1.64f) * (1.0f - ratio)) + (f3 * ratio));

        // If we need to check sanity, check it, in gears 2,3 and 4, RPM readings should be the same,
        // otherwise we have a faulty conductor place sensor!
        sane = abs(n2Rpm - n3Rpm) < 150;
    }

    // Relative RPM trend (not calibrated to real speed) - enough to tell accelerating from decelerating.
    if (lastCalcRpm >= 0)
        rpmTrendPerSec = (int)((calcRpm - lastCalcRpm) * 1000.0f / (float)deltaMs);
    lastCalcRpm = calcRpm;
    lastN2Pulses = n2Pulses;
    lastN3Pulses = n3Pulses;
    lastN2Rpm = n2Rpm;
    lastN3Rpm = n3Rpm;

    // Throttled diagnostic print - raw pulse counts are the ground truth for confirming the sensors
    // are actually wired/counting; the derived RPMs/trend confirm the gear-ratio math on top of that.
    static uint32_t lastRpmDebugMs = 0;
    if (millis() - lastRpmDebugMs >= 500) {
        lastRpmDebugMs = millis();
        Serial.printf("[N2N3] n2Pulses=%d n3Pulses=%d n2Rpm=%d n3Rpm=%d calcRpm=%d trend=%d/s sane=%d\n",
            n2Pulses, n3Pulses, n2Rpm, n3Rpm, calcRpm, rpmTrendPerSec, sane);
    }

    return check_sanity ? sane : true;
}

int Sensors::GetRpmTrendPerSec() {
    return rpmTrendPerSec;
}

int Sensors::GetLastN2Pulses() {
    return lastN2Pulses;
}

int Sensors::GetLastN3Pulses() {
    return lastN3Pulses;
}

int Sensors::GetLastN2Rpm() {
    return lastN2Rpm;
}

int Sensors::GetLastN3Rpm() {
    return lastN3Rpm;
}

int Sensors::GetLastCalculatedRpm() {
    return lastCalcRpm;
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

bool Sensors::read_throttle_position(int* dest, const VaLas_Controller::ThrottleCalibration& settings) {
    int rawAdc = ReadThrottleAdc();
    if (rawAdc < 0 || settings.wideOpenAdc <= settings.closedAdc ||
        settings.wideOpenAdc - settings.closedAdc < 100) {
        return false;
    }

    *dest = constrain(map(rawAdc, settings.closedAdc, settings.wideOpenAdc, 0, 100), 0, 100);
    return true;
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
