#ifndef _Analog_H
#define _Analog_H

#include "gpaddon.h"
#include "GamepadEnums.h"
#include "BoardConfig.h"
#include "enums.pb.h"
#include "types.h"

// config_utilsが旧ADC設定の既定値を参照するため、マクロは残す。
#ifndef ANALOG_INPUT_ENABLED
#define ANALOG_INPUT_ENABLED 0
#endif

#ifndef ANALOG_ADC_1_VRX
#define ANALOG_ADC_1_VRX    -1
#endif

#ifndef ANALOG_ADC_1_VRY
#define ANALOG_ADC_1_VRY    -1
#endif

#ifndef ANALOG_ADC_1_MODE
#define ANALOG_ADC_1_MODE DPAD_MODE_LEFT_ANALOG
#endif

#ifndef ANALOG_ADC_1_INVERT
#define ANALOG_ADC_1_INVERT INVERT_NONE
#endif

#ifndef ANALOG_ADC_2_VRX
#define ANALOG_ADC_2_VRX    -1
#endif

#ifndef ANALOG_ADC_2_VRY
#define ANALOG_ADC_2_VRY    -1
#endif

#ifndef ANALOG_ADC_2_MODE
#define ANALOG_ADC_2_MODE DPAD_MODE_RIGHT_ANALOG
#endif

#ifndef ANALOG_ADC_2_INVERT
#define ANALOG_ADC_2_INVERT INVERT_NONE
#endif

#ifndef FORCED_CIRCULARITY_ENABLED
#define FORCED_CIRCULARITY_ENABLED 0
#endif

#ifndef FORCED_CIRCULARITY2_ENABLED
#define FORCED_CIRCULARITY2_ENABLED 0
#endif

#ifndef DEFAULT_INNER_DEADZONE
#define DEFAULT_INNER_DEADZONE 5
#endif

#ifndef DEFAULT_INNER_DEADZONE2
#define DEFAULT_INNER_DEADZONE2 5
#endif

#ifndef DEFAULT_OUTER_DEADZONE
#define DEFAULT_OUTER_DEADZONE 95
#endif

#ifndef DEFAULT_OUTER_DEADZONE2
#define DEFAULT_OUTER_DEADZONE2 95
#endif

#ifndef AUTO_CALIBRATE_ENABLED
#define AUTO_CALIBRATE_ENABLED 0
#endif

#ifndef AUTO_CALIBRATE2_ENABLED
#define AUTO_CALIBRATE2_ENABLED 0
#endif

#ifndef ANALOG_SMOOTHING_ENABLED
#define ANALOG_SMOOTHING_ENABLED 0
#endif

#ifndef ANALOG_SMOOTHING2_ENABLED
#define ANALOG_SMOOTHING2_ENABLED 0
#endif

#ifndef SMOOTHING_FACTOR
#define SMOOTHING_FACTOR 5
#endif

#ifndef SMOOTHING_FACTOR2
#define SMOOTHING_FACTOR2 5
#endif

#ifndef ANALOG_ERROR
#define ANALOG_ERROR 1000
#endif

#ifndef ANALOG_ERROR2
#define ANALOG_ERROR2 1000
#endif

// Analog Module Name
#define AnalogName "Analog"

#define ADC_COUNT 2

typedef struct
{
    uint8_t x_center = 128;
    uint8_t y_center = 128;
    bool calibrated = false;
    InvertMode analog_invert;
    DpadMode analog_dpad;
    float x_ema = 0.5f;
    float y_ema = 0.5f;
    bool ema_option;
    float ema_smoothing;
    float error_rate;
    float in_deadzone;
    float out_deadzone;
    bool auto_calibration;
    bool forced_circularity;
} analog_instance;

class AnalogInput : public GPAddon {
public:
    virtual bool available();
    virtual void setup();       // Analog Setup
    virtual void process();     // Analog Process
    virtual void preprocess();
    virtual void postprocess(bool sent) {}
    virtual void reinit() { setup(); }
    virtual std::string name() { return AnalogName; }
private:
    void resetCalibration();
    void processStick(int stick_num, uint8_t raw_x, uint8_t raw_y, bool one_axis,
        float &x, float &y);
    analog_instance sticks[ADC_COUNT];
    uint32_t pressure_modes[3] = {12, 11, 0};
    uint32_t last_generation = 0;
    uint8_t last_id = 0;
    uint8_t owned_outputs = 0;
    bool last_connected = false;
    bool last_analog = false;
    bool last_negcon = false;
};

#endif  // _Analog_H_
