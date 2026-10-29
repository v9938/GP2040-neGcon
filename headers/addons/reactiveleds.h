#ifndef REACTIVELEDS_H_
#define REACTIVELEDS_H_

#include "gpaddon.h"

#ifndef REACTIVE_LED_ENABLED
#define REACTIVE_LED_ENABLED 0
#endif

#ifndef REACTIVE_LED_COUNT
#define REACTIVE_LED_COUNT 3
#endif

#ifndef REACTIVE_LED_DELAY
#define REACTIVE_LED_DELAY 1
#endif

#ifndef REACTIVE_LED_MAX_BRIGHTNESS
#define REACTIVE_LED_MAX_BRIGHTNESS 255
#endif

#ifndef REACTIVE_LED_FADE_INC
#define REACTIVE_LED_FADE_INC 1
#endif

// Reactive LED Module
#define ReactiveLEDName "ReactiveLED"

// Reactive LED
class ReactiveLEDAddon : public GPAddon
{
    public:
        virtual bool available();
        virtual void setup();
        virtual void preprocess() {}
        virtual void process();
        virtual void postprocess(bool sent) {}
        virtual void reinit() { setup(); }
        virtual std::string name() { return ReactiveLEDName; }
    private:
        struct ReactiveLEDPinState {
            ReactiveLEDMode modeDown = ReactiveLEDMode::REACTIVE_LED_STATIC_ON;
            ReactiveLEDMode modeUp = ReactiveLEDMode::REACTIVE_LED_STATIC_OFF;
            GpioAction action = GpioAction::NONE;
            uint8_t value = 0;
            uint8_t brightness = 255;
            bool currState = false;
            bool prevState = false;
            bool initialized = false;
            uint32_t lastUpdate = 0;
            uint32_t currUpdate = 0;
        };

        ReactiveLEDPinState ledPins[REACTIVE_LED_COUNT];
        bool enabled = false;
        uint32_t lastProcess = 0;

        void setLEDByMode(ReactiveLEDPinState &ledState, uint8_t level);
};

#endif
