#ifndef CPINS_H
#define CPINS_H

#include <HardwareTimer.h>
#include <Arduino.h>

#define PWMMAX 256

#ifndef CPIN_HWFREQ
#define CPIN_HWFREQ 1000
#endif

enum cpinledtype { CPIN_PIN = 0, CPIN_LED = 1, CPIN_HWPIN = 2, CPIN_HWLED = 3 };
enum cpinmode { CPIN_NORMAL = 0, CPIN_ADDITIVE, CPIN_CONTINUE, CPIN_MODEEND };
enum cpinstate { CPIN_LOW = 0, CPIN_HIGH = 1, CPIN_OD = 2 };

static const uint8_t pwmLED[257] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05,
    0x05, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x09, 0x09,
    0x0A, 0x0A, 0x0B, 0x0B, 0x0C, 0x0C, 0x0D, 0x0D, 0x0E, 0x0F, 0x0F, 0x10,
    0x11, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
    0x1C, 0x1D, 0x1F, 0x20, 0x21, 0x23, 0x24, 0x26, 0x27, 0x29, 0x2B, 0x2C,
    0x2E, 0x30, 0x32, 0x34, 0x36, 0x38, 0x3A, 0x3C, 0x3E, 0x40, 0x43, 0x45,
    0x47, 0x4A, 0x4C, 0x4F, 0x51, 0x54, 0x57, 0x59, 0x5C, 0x5F, 0x62, 0x64,
    0x67, 0x6A, 0x6D, 0x70, 0x73, 0x76, 0x79, 0x7C, 0x7F, 0x82, 0x85, 0x88,
    0x8B, 0x8E, 0x91, 0x94, 0x97, 0x9A, 0x9C, 0x9F, 0xA2, 0xA5, 0xA7, 0xAA,
    0xAD, 0xAF, 0xB2, 0xB4, 0xB7, 0xB9, 0xBB, 0xBE, 0xC0, 0xC2, 0xC4, 0xC6,
    0xC8, 0xCA, 0xCC, 0xCE, 0xD0, 0xD2, 0xD3, 0xD5, 0xD7, 0xD8, 0xDA, 0xDB,
    0xDD, 0xDE, 0xDF, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9,
    0xEA, 0xEB, 0xEC, 0xED, 0xED, 0xEE, 0xEF, 0xEF, 0xF0, 0xF1, 0xF1, 0xF2,
    0xF2, 0xF3, 0xF3, 0xF4, 0xF4, 0xF5, 0xF5, 0xF6, 0xF6, 0xF6, 0xF7, 0xF7,
    0xF7, 0xF8, 0xF8, 0xF8, 0xF9, 0xF9, 0xF9, 0xF9, 0xFA, 0xFA, 0xFA, 0xFA,
    0xFA, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC,
    0xFC, 0xFC, 0xFC, 0xFC, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD,
    0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
    0xFE, 0xFE, 0xFF, 0xFF, 0xFF};

class cPins {
private:
  GPIO_TypeDef *port; // lowlevel GPIO port
  uint16_t pin;       // lowlevel GPIO pin
  uint32_t pinHW;     // arduino pin num
#ifndef CPIN_DISABLE_HWTIMER
  uint32_t pinFrequency = CPIN_HWFREQ; // default frequency for HW pin
#endif
  uint8_t isHW = 0;            // is hardware pwm?
  uint8_t highState, lowState; // pin states gpio level
  uint16_t pwmDuty = 0;        // pwm duty cycle value after led correction
  uint16_t Duty =
      0; // pwm duty cycle before led correction (=pwmDuty if not led)
  uint32_t blinkPeriod = 0;        // period of blink/breath etc
  uint32_t toBreathe = 0;          // is configured as breathe
  volatile uint32_t blinkTime = 0; // time of blink/breath etc
  uint32_t blinkInitTime = 0;      // container for time initial time value
  uint8_t mode = CPIN_NORMAL;      // initial mode, no addition or phase shift
  uint8_t blinkFade = 0;           // is configured as blink&fade
  uint8_t isLed = 0;               // is configured as led
  uint32_t phaseShift = 0;         // phase shift value
  bool noInterruptable = false;    // thread safe mutex
  static uint32_t prevms;          // previous millisecond value
  static uint8_t ledBrightness;    // global brightness for leds
  void setTime(uint32_t time); // set pin time value depending on current mode

public:
#ifndef CPIN_DISABLE_HWTIMER
  static HardwareTimer *T; // hardware timer object
  static bool timerInited; // is timer inited or not
#endif
  static uint64_t timerCounter;                 // pin instances counter
  static uint32_t timerFreq;                    // timer frequency
  static const uint32_t ticksPerCycle = PWMMAX; // timer ticks per duty cycle
#undef __inst__
#define __inst__ std::vector<cPins *>
  inline static __inst__ localinstances = {};
  inline static __inst__ instances = {};
  __inst__::iterator _me;
  char *name = (char *)NULL;  // pin name, filled by CPIN define, can be used
                              // for debug purposes
  uint16_t tempDuty = 0;      // current calculated duty for pin
  uint16_t prevDuty = 0xFFFF; // previous calculated duty

  cPins(const char *cname, uint16_t _PIN, uint8_t isled = CPIN_PIN,
        uint8_t hs = CPIN_HIGH);
  // CPIN_PIN will init pin as usual gpio, use CPIN_LED to make brightness
  // change better for leds hs = 0 is for active low pins

  /* direct pin access routines, don't use it without special needs */
  __always_inline void write(uint8_t value);
  __always_inline void reset(void);
  __always_inline void set(void);
  __always_inline void toggle(void);
  __always_inline uint8_t read(void);

  bool isActive(void);         // return false when its off
  uint32_t getRemaining(void); // return 0 when its off and ms value when on
  uint32_t getTime(void);      // return working time given at start

  void setPWM(uint16_t duty); // set PWM duty for pin

  uint16_t getDuty(void); // for internal use

  void blink(
      uint32_t time,
      uint32_t period = 0); // blink, 1/2 of period it will be on, 1/2 of period
                            // - off, if period is 0 its equal to .on(time)

  void breathe(uint32_t time,
               uint32_t period = 0); // breathing, 1/2 period softly increasing
                                     // brightness and decreasing after

  void blinkfade(
      uint32_t time,
      uint32_t period = 0); // 1/2 period will be ON, next 1/2 will fadeout

  void on(uint32_t time = ~0UL); // just switch it on for a specific time

  void off(); // switch off

  void setMode(
      uint8_t newMode); // normally any new call will reset previous state
                        // CPIN_CONTINUE allows to set new time without changing
                        // period CPIN_ADDITIVE will add time with each new call

  void noInterrupt(void); // make pin noninterruptable, any new calls will be
                          // ignored till previously set job will expire

  static void setBrightness(
      uint8_t brightness); // set global brightness (for CPIN_LED pins only)

  void setPhase(uint32_t phase) {
    phaseShift = phase;
  } // manually set phase shift
#ifndef CPIN_DISABLE_HWTIMER
  void setFrequency(uint32_t frequency);
#endif

#ifndef CPIN_DISABLE_HWTIMER
  static void timerCallback(void); // callback for timer routines

  static void
  initTimer(TIM_TypeDef *timer = TIM1,
            uint32_t freq = 60); // by default it will use TIM1 at 60Hz, dont
                                 // forget to init it at startup

  static void freeTimer(void);
#else
  static void timerCallback(void); // callback for timer routines
#endif
  std::function<void(cPins *pin)> finishCallback = [](cPins *) {
  }; // callback called after time expiring or off command
  std::function<void(cPins *pin)> tickCallback = [](cPins *) {
  }; // callback called every ms
  ~cPins();
};

#define CPIN(a, ...) cPins(a)(#a, __VA_ARGS__)

#endif
