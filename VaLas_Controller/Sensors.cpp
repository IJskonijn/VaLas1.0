#include <Arduino.h>
#include "Sensors.h"
#include "VaLas_Controller.h"
#include "driver/pcnt.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver/timer.h"

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

// PCNT configurations
const pcnt_config_t pcnt_cfg_n2 {
    .pulse_gpio_num = PIN_N2,
    .ctrl_gpio_num = PCNT_PIN_NOT_USED,
    .lctrl_mode = PCNT_MODE_KEEP,
    .hctrl_mode = PCNT_MODE_KEEP,
    .pos_mode = PCNT_COUNT_INC,
    .neg_mode = PCNT_COUNT_INC,
    .counter_h_lim = PCNT_H_LIM,
    .counter_l_lim = 0,
    .unit = PCNT_N2_RPM,
    .channel = PCNT_CHANNEL_0
};

const pcnt_config_t pcnt_cfg_n3 {
    .pulse_gpio_num = PIN_N3,
    .ctrl_gpio_num = PCNT_PIN_NOT_USED,
    .lctrl_mode = PCNT_MODE_KEEP,
    .hctrl_mode = PCNT_MODE_KEEP,
    .pos_mode = PCNT_COUNT_INC,
    .neg_mode = PCNT_COUNT_INC,
    .counter_h_lim = PCNT_H_LIM,
    .counter_l_lim = 0,
    .unit = PCNT_N3_RPM,
    .channel = PCNT_CHANNEL_0
};

static intr_handle_t rpm_timer_handle;
static void IRAM_ATTR RPM_TIMER_ISR(void* arg) {
    TIMERG0.int_clr_timers.t0 = 1;
    TIMERG0.hw_timer[0].config.alarm_en = 1;
    int16_t t = 0;
    portENTER_CRITICAL_ISR(&n3_mux);
    pcnt_get_counter_value(PCNT_N3_RPM, &t);
    pcnt_counter_clear(PCNT_N3_RPM);
    t += (n3_overflow * PCNT_H_LIM);
    n3_overflow = 0;
    n3_rpm = t;
    portEXIT_CRITICAL_ISR(&n3_mux);

    portENTER_CRITICAL_ISR(&n2_mux);
    pcnt_get_counter_value(PCNT_N2_RPM, &t);
    pcnt_counter_clear(PCNT_N2_RPM);
    t += (n2_overflow * PCNT_H_LIM);
    n2_overflow = 0;
    n2_rpm = t;
    portEXIT_CRITICAL_ISR(&n2_mux);
}

static void IRAM_ATTR on_pcnt_overflow_n2(void* args) {
    portENTER_CRITICAL_ISR(&n2_mux);
    n2_overflow++;
    portEXIT_CRITICAL_ISR(&n2_mux);
}

static void IRAM_ATTR on_pcnt_overflow_n3(void* args) {
    portENTER_CRITICAL_ISR(&n3_mux);
    n3_overflow++;
    portEXIT_CRITICAL_ISR(&n3_mux);
}

#define CHECK_ESP_FUNC(x, msg, ...) \
res = x; \
if (res != ESP_OK) { \
    ESP_LOGE(LOG_TAG, msg, ##__VA_ARGS__); \
    return false; \
}   \


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
  // Read stock OM606 rpm sensor here
  // Or read stock OM603 rpm sensor here
  // OM603 Calculation: frequency / 144 (flywheel tooth) * 60 = RPM.
  // OM606 Calculation: frequency / 6 (flywheel tabs) * 60 = RPM.
  
  // If using a 300E rpm gauge, it needs 3 pulses per rpm
  // If using a 300D rpm gauge, it needs 144 pulses per rpm
  // OM606 sensor + 300E gauge == frequency / 2?

  //rpmRevs = rpmPulse / config.triggerWheelTeeth / elapsedTime * 1000 * 60;

  // if (rpmSpeed)
  //   {
  //     // speed based on engine rpm
  //     vehicleSpeedRPM = tireCircumference * curRPM / (ratioFromGear(gear) * config.diffRatio) / 1000000 * 60;
  //     speedValue = vehicleSpeedRPM;
  //   } 
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


bool Sensors::init_sensors(){
    esp_err_t res;
    CHECK_ESP_FUNC(gpio_set_direction(PIN_ATF, GPIO_MODE_INPUT), "Failed to set PIN_ATF to Input! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(gpio_set_direction(PIN_N2, GPIO_MODE_INPUT), "Failed to set PIN_N2 to Input! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(gpio_set_direction(PIN_N3, GPIO_MODE_INPUT), "Failed to set PIN_N3 to Input! %s", esp_err_to_name(res))

    // Set RPM pins to pullup
    CHECK_ESP_FUNC(gpio_set_pull_mode(PIN_N2, GPIO_PULLUP_ONLY), "Failed to set PIN_N2 to Input! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(gpio_set_pull_mode(PIN_N3, GPIO_PULLUP_ONLY), "Failed to set PIN_N3 to Input! %s", esp_err_to_name(res))

    // Now configure PCNT to begin counting!
    CHECK_ESP_FUNC(pcnt_unit_config(&pcnt_cfg_n2), "Failed to configure PCNT for N2 RPM reading! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_unit_config(&pcnt_cfg_n3), "Failed to configure PCNT for N3 RPM reading! %s", esp_err_to_name(res))

    // Pause PCNTs unit configuration is complete
    CHECK_ESP_FUNC(pcnt_counter_pause(PCNT_N2_RPM), "Failed to pause PCNT N2 RPM! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_counter_pause(PCNT_N3_RPM), "Failed to pause PCNT N3 RPM! %s", esp_err_to_name(res))

    // Clear their stored values (If present)
    CHECK_ESP_FUNC(pcnt_counter_clear(PCNT_N2_RPM), "Failed to clear PCNT N2 RPM! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_counter_clear(PCNT_N3_RPM), "Failed to clear PCNT N3 RPM! %s", esp_err_to_name(res))

    // Setup filter to ignore ultra short pulses (possibly noise)
    // Using a value of 40 at 80Mhz APB_CLOCK = this will correctly filter noise up to 30,000RPM 
    CHECK_ESP_FUNC(pcnt_set_filter_value(PCNT_N2_RPM, 1000), "Failed to set filter for PCNT N2! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_set_filter_value(PCNT_N3_RPM, 1000), "Failed to set filter for PCNT N3! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_filter_enable(PCNT_N2_RPM), "Failed to enable filter for PCNT N2! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_filter_enable(PCNT_N3_RPM), "Failed to enable filter for PCNT N3! %s", esp_err_to_name(res))

    // Now install and register ISR interrupts
    CHECK_ESP_FUNC(pcnt_isr_service_install(0), "Failed to install ISR service for PCNT! %s", esp_err_to_name(res))

    CHECK_ESP_FUNC(pcnt_isr_handler_add(PCNT_N2_RPM, &on_pcnt_overflow_n2, nullptr), "Failed to add PCNT N2 to ISR handler! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_isr_handler_add(PCNT_N3_RPM, &on_pcnt_overflow_n3, nullptr), "Failed to add PCNT N3 to ISR handler! %s", esp_err_to_name(res))

    // Enable interrupts on hitting hlim on PCNTs
    CHECK_ESP_FUNC(pcnt_event_enable(PCNT_N2_RPM, pcnt_evt_type_t::PCNT_EVT_H_LIM), "Failed to register event for PCNT N2! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_event_enable(PCNT_N3_RPM, pcnt_evt_type_t::PCNT_EVT_H_LIM), "Failed to register event for PCNT N3! %s", esp_err_to_name(res))

    // Resume counting
    CHECK_ESP_FUNC(pcnt_counter_resume(PCNT_N2_RPM), "Failed to resume PCNT N2 RPM! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_counter_resume(PCNT_N3_RPM), "Failed to resume PCNT N3 RPM! %s", esp_err_to_name(res))

    timer_config_t config = {
            .alarm_en = timer_alarm_t::TIMER_ALARM_EN,
            .counter_en = timer_start_t::TIMER_START,
            .intr_type = TIMER_INTR_LEVEL,
            .counter_dir = TIMER_COUNT_UP,
            .auto_reload = timer_autoreload_t::TIMER_AUTORELOAD_EN,
            .divider = 80   /* 1 us per tick */
    };
    CHECK_ESP_FUNC(timer_init(TIMER_GROUP_0, TIMER_0, &config), "Failed to init timer 0 for PCNT measuring! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0), "%s", esp_err_to_name(res))
    CHECK_ESP_FUNC(timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, TIMER_INTERVAL_MS*1000), "%s", esp_err_to_name(res))
    CHECK_ESP_FUNC(timer_enable_intr(TIMER_GROUP_0, TIMER_0), "%s", esp_err_to_name(res))
    CHECK_ESP_FUNC(timer_isr_register(TIMER_GROUP_0, TIMER_0, &RPM_TIMER_ISR, NULL, 0, &rpm_timer_handle), "%s", esp_err_to_name(res))
    CHECK_ESP_FUNC(timer_start(TIMER_GROUP_0, TIMER_0), "%s", esp_err_to_name(res))
    ESP_LOGI(LOG_TAG, "Sensors INIT OK!");
    return true;
}

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
        if (raw >= 1000) {
            return false; // Parking lock engaged, cannot read.
        }
        avg += raw;
    }
    avg /= NUM_ATF_SAMPLES;
    //ESP_LOGI("ATF", "AVG VOLTAGE %d", avg);
    if (avg < atf_temp_lookup[0].v) {
        *dest = atf_temp_lookup[0].temp;
        return true;
    } else if (avg > atf_temp_lookup[NUM_TEMP_POINTS-1].v) {
        *dest = (atf_temp_lookup[NUM_TEMP_POINTS-1].temp) / 10;
        return true;
    } else {
        for (uint8_t i = 0; i < NUM_TEMP_POINTS-1; i++) {
            // Found! Interpolate linearly to get a better estimate of ATF Temp
            if (atf_temp_lookup[i].v <= avg && atf_temp_lookup[i+1].v >= avg) {
                float dx = avg - atf_temp_lookup[i].v;
                float dy = atf_temp_lookup[i+1].v - atf_temp_lookup[i].v;
                *dest = (atf_temp_lookup[i].temp + (atf_temp_lookup[i+1].temp-atf_temp_lookup[i].temp) * ((dx)/dy)) / 10;
                return true;
            }
        }
        return true;
    }
}
