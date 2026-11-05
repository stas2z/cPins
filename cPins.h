#ifndef CPINS_H
#define CPINS_H

#include <HardwareTimer.h>
#include <functional>
#include <vector>
#include <sketch.h>

#define PWMMAX 256

#ifndef CPIN_HWFREQ
#define CPIN_HWFREQ 1000
#endif

enum cpinledtype { CPIN_PIN = 0, CPIN_LED = 1, CPIN_HWPIN = 2, CPIN_HWLED = 3 };
enum cpinmode { CPIN_NORMAL = 0, CPIN_ADDITIVE, CPIN_CONTINUE, CPIN_MODEEND };
enum cpinstate { CPIN_LOW = 0, CPIN_HIGH = 1, CPIN_OD = 2 };

inline constexpr uint8_t pwmLED[257] = {
    0,   0,   0,   0,   1,   1,   1,   1,   2,   2,   2,   3,   3,   3,   4,
    4,   4,   5,   5,   6,   6,   6,   7,   7,   8,   8,   8,   9,   9,   10,
    10,  11,  11,  12,  12,  13,  13,  14,  14,  15,  15,  16,  16,  17,  18,
    18,  19,  19,  20,  20,  21,  22,  22,  23,  24,  24,  25,  25,  26,  27,
    27,  28,  29,  30,  30,  31,  32,  32,  33,  34,  35,  35,  36,  37,  38,
    38,  39,  40,  41,  42,  42,  43,  44,  45,  46,  46,  47,  48,  49,  50,
    51,  52,  53,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,
    65,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  78,  79,
    80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  93,  94,  95,
    96,  97,  98,  99,  100, 102, 103, 104, 105, 106, 107, 109, 110, 111, 112,
    113, 114, 116, 117, 118, 119, 121, 122, 123, 124, 125, 127, 128, 129, 130,
    132, 133, 134, 135, 137, 138, 139, 141, 142, 143, 144, 146, 147, 148, 150,
    151, 152, 154, 155, 156, 158, 159, 160, 161, 163, 164, 166, 167, 168, 170,
    171, 172, 174, 175, 176, 178, 179, 180, 182, 183, 185, 186, 187, 189, 190,
    191, 193, 194, 196, 197, 198, 200, 201, 203, 204, 205, 207, 208, 210, 211,
    212, 214, 215, 217, 218, 220, 221, 222, 224, 225, 227, 228, 230, 231, 232,
    234, 235, 237, 238, 240, 241, 242, 244, 245, 247, 248, 250, 251, 252, 254,
    255, 255};

class cPins {
private:
  GPIO_TypeDef *port; // lowlevel GPIO port
  uint16_t pin;       // lowlevel GPIO pin
  uint32_t pinHW;     // arduino pin num
#ifndef CPIN_DISABLE_HWTIMER
  uint32_t pinFrequency = CPIN_HWFREQ; // default frequency for HW pin
#endif
  uint8_t isHW = 0;     // is hardware pwm?
  uint16_t pwmDuty = 0; // pwm duty cycle value after led correction
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
  static uint32_t subTicks;        // ms accumulator (in 1/1000 of tick)
  static_assert((PWMMAX & (PWMMAX - 1)) == 0, "PWMMAX must be power of two");
  bool tryBegin(void); // false if pin is noInterruptable and busy
  void finish(void);   // stop output and call finishCallback
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
  uint8_t highState, lowState;                  // pin states gpio level
  // public API kept for compatibility. instances: all pins (changed only in
  // ctor/dtor). localinstances: currently active pins, refreshed every ms by
  // the timer (capacity is reserved in ctor, so no allocation in ISR).
  inline static std::vector<cPins *> localinstances = {};
  inline static std::vector<cPins *> instances = {};
  char name[32] = "";         // pin name, filled by CPIN define, can be used
                              // for debug purposes
  uint16_t tempDuty = 0;      // current calculated duty for pin
  uint16_t prevDuty = 0xFFFF; // previous calculated duty

  cPins(const char *cname, uint16_t _PIN, uint8_t isled = CPIN_PIN,
        uint8_t hs = CPIN_HIGH);
  // CPIN_PIN will init pin as usual gpio, use CPIN_LED to make brightness
  // change better for leds hs = 0 is for active low pins

  /* direct pin access routines, don't use it without special needs */
  __always_inline void write(uint8_t value) {
    port->BSRR = (uint32_t)pin << (!(value & 1U) << 4);
  }
  __always_inline void reset(void) { write(lowState); }
  __always_inline void set(void) { write(highState); }
  __always_inline void toggle(void) { // atomic via BSRR
    port->BSRR = (port->ODR & pin) ? ((uint32_t)pin << 16) : (uint32_t)pin;
  }
  // only for output ports, returns 0/1 (physical level)
  __always_inline uint8_t read(void) { return (port->ODR & pin) != 0; }

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
  // Callbacks run in ISR context (keep them short).
  std::function<void(cPins *pin)> finishCallback = [](cPins *) {
  }; // callback called after time expiring or off command
  std::function<void(cPins *pin)> tickCallback = [](cPins *) {
  }; // callback called every ms
  cPins(const cPins &) = delete;
  cPins &operator=(const cPins &) = delete;
  ~cPins();
};

#define CPIN(a, ...) cPins a(#a, __VA_ARGS__)

#endif
