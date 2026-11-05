#include <wiring.h>

#include "stm32_def.h"
#include "stm32yyxx_ll_gpio.h"

#ifndef CPIN_DISABLE_HWTIMER
#include <HardwareTimer.h>
#endif

#include "cPins.h"

#include <algorithm>

uint64_t cPins::timerCounter = 0;
uint32_t cPins::timerFreq = 1;
uint32_t cPins::subTicks = 0;
uint8_t cPins::ledBrightness = 100;
#ifndef CPIN_DISABLE_HWTIMER
HardwareTimer *cPins::T = nullptr;
bool cPins::timerInited = false;
#endif

namespace {
// short critical section, safe to nest and to use from ISR
struct CritSec {
  uint32_t primask;
  CritSec() : primask(__get_PRIMASK()) { __disable_irq(); }
  ~CritSec() { __set_PRIMASK(primask); }
};
inline uint32_t fixPeriod(uint32_t p) { return p == 1 ? 2 : p; }
} // namespace

cPins::cPins(const char *cname, uint16_t _PIN, uint8_t isled, uint8_t hs)
    : port(get_GPIO_Port(STM_PORT(digitalPinToPinName(_PIN)))),
      pin(STM_GPIO_PIN(digitalPinToPinName(_PIN))), pinHW(_PIN),
      isHW(!!(isled & 2U)), isLed(isled & 1U), highState(hs & 1U),
      lowState(!(hs & 1U)) {
  uint8_t isOD = hs & 2U;
  uint32_t ll_pin = STM_LL_GPIO_PIN(digitalPinToPinName(pinHW));
  set_GPIO_Port_Clock(STM_PORT(digitalPinToPinName(pinHW)));
  LL_GPIO_SetPinMode(port, ll_pin, LL_GPIO_MODE_OUTPUT);
  LL_GPIO_SetPinOutputType(
      port, ll_pin, isOD ? LL_GPIO_OUTPUT_OPENDRAIN : LL_GPIO_OUTPUT_PUSHPULL);
#ifndef STM32F1xx
  LL_GPIO_SetPinPull(port, ll_pin, LL_GPIO_PULL_NO);
#endif
  LL_GPIO_SetPinSpeed(port, ll_pin, LL_GPIO_SPEED_FREQ_HIGH);
#ifndef CPIN_DISABLE_HWTIMER
  if ((isHW) && !(pin_in_pinmap(digitalPinToPinName(_PIN), PinMap_PWM)))
#endif
  {
    isHW = 0;
  }
  reset();
  strncpy(name, cname, sizeof(name) - 1);
  name[sizeof(name) - 1] = 0;
  setPWM(PWMMAX);
  CritSec cs;
  instances.push_back(this);
  localinstances.reserve(instances.size()); // so ISR never allocates
}

// only to read output ports, input ports should be read from IDR
bool cPins::isActive(void) { return (blinkTime != 0); }
uint32_t cPins::getRemaining(void) { return blinkTime; }
uint32_t cPins::getTime(void) { return blinkInitTime; }

// must be called with interrupts disabled
void cPins::setTime(uint32_t time) {
  if (!blinkTime) { // if pin finished any job already, just init it as NORMAL
    blinkInitTime = blinkTime = time;
    phaseShift = 0;
    return;
  }
  switch (mode) {
  default:
  case CPIN_NORMAL:
    blinkTime = blinkInitTime = time;
    phaseShift = 0;
    break;
  case CPIN_ADDITIVE: // simply add some extra time to it, saturating
    blinkTime = (time > ~0UL - blinkTime) ? ~0UL : blinkTime + time;
    blinkInitTime = (time > ~0UL - blinkInitTime) ? ~0UL : blinkInitTime + time;
    break;
  case CPIN_CONTINUE:
    if (blinkPeriod) // shift of phase to continue at the same state
      phaseShift = (blinkInitTime - blinkTime + phaseShift) % (blinkPeriod);
    else
      phaseShift = 0;
    blinkInitTime = blinkTime = time;
    break;
  }
}
void cPins::noInterrupt(void) { noInterruptable = true; }
void cPins::setMode(uint8_t newMode) {
  if (newMode < CPIN_MODEEND)
    mode = newMode;
}

void cPins::setPWM(uint16_t duty) {
  if (duty <= PWMMAX) {
    Duty = duty;
    pwmDuty = isLed ? pwmLED[((ledBrightness * (Duty << 8)) / 100) >> 8]
                    : Duty; // magic duty calcs for led :)
  }
}
// call with interrupts disabled
bool cPins::tryBegin(void) {
  if (noInterruptable && blinkTime)
    return false;
  noInterruptable = false;
  return true;
}
void cPins::blink(uint32_t time, uint32_t period) {
  CritSec cs;
  if (!tryBegin())
    return;
  setTime(time);
  blinkPeriod = fixPeriod(period);
  toBreathe = 0;
}
void cPins::breathe(uint32_t time, uint32_t period) {
  CritSec cs;
  if (!tryBegin())
    return;
  setTime(time);
  blinkPeriod = fixPeriod(period ? period : time);
  toBreathe = 1;
  blinkFade = 0;
}
void cPins::blinkfade(uint32_t time, uint32_t period) {
  CritSec cs;
  if (!tryBegin())
    return;
  setTime(time);
  blinkPeriod = fixPeriod(period ? period : time);
  toBreathe = 1;
  blinkFade = 1;
}
void cPins::on(uint32_t time) {
  CritSec cs;
  if (!tryBegin())
    return;
  setTime(time);
  blinkPeriod = 0;
  toBreathe = 0;
}
void cPins::off() {
  {
    CritSec cs;
    if (!tryBegin())
      return;
    blinkTime = 0;
  }
  finish(); // stops output, calls callback (once, as before even if idle)
}
uint16_t cPins::getDuty(void) { return (Duty); }

void cPins::setBrightness(uint8_t brightness) {
  ledBrightness = brightness > 100 ? 100 : brightness;
  for (cPins *p : instances) {
    if (p->isLed == CPIN_LED)
      p->setPWM(p->getDuty());
  }
}

// blinkTime is already 0 here. Called from ISR or from off().
void cPins::finish(void) {
#ifndef CPIN_DISABLE_HWTIMER
  if (isHW && prevDuty > 0) {
    pwm_start(digitalPinToPinName(pinHW), pinFrequency,
              lowState ? PWMMAX - 1 : 0, (TimerCompareFormat_t)8);
    prevDuty = 0;
  }
#endif
  reset();
  blinkPeriod = 0;
  toBreathe = 0;
  blinkFade = 0;
  blinkInitTime = 0;
  if (finishCallback)
    finishCallback(this);
}

__attribute__((optimize(3))) void cPins::timerCallback(void) {
  const uint32_t counter = (uint32_t)timerCounter & (ticksPerCycle - 1);
  timerCounter++;

  // millisecond bookkeeping without 64bit division
  uint32_t delta = 0;
  const uint32_t ticksPerSec = timerFreq * ticksPerCycle;
  subTicks += 1000;
  if (ticksPerSec && subTicks >= ticksPerSec) {
    delta = subTicks / ticksPerSec;
    subTicks -= delta * ticksPerSec;
  }

  if (delta)
    localinstances.clear();
  // index loop: instances may not be modified from callbacks
  for (size_t i = 0, n = instances.size(); i < n; i++) {
    cPins *p = instances[i];
    if (delta) {
      if (p->blinkTime) {
        if (p->blinkTime > delta) {
          p->blinkTime -= delta;
        } else {
          p->blinkTime = 0;
          p->finish();
        }
      }
      if (p->tickCallback)
        p->tickCallback(p);
      if (p->blinkTime && localinstances.size() < localinstances.capacity())
        localinstances.push_back(p);
    }
    if (!p->blinkTime)
      continue;

    if (!counter) {
      if (p->blinkPeriod) {
        const uint32_t half = p->blinkPeriod >> 1; // >= 1, see fixPeriod
        const uint32_t elapsed =
            p->blinkInitTime - p->blinkTime + 1 + p->phaseShift;
        const uint32_t q = elapsed / half;
        const uint32_t period = q & 1UL;
        if (p->toBreathe) {
          const uint32_t tduty = p->pwmDuty; // already led-corrected
          if ((!period) && p->blinkFade) {
            p->tempDuty = tduty;
          } else {
            const uint32_t step = elapsed - q * half;
            const uint32_t d = (((tduty << 8) / half) * step) >> 8;
            p->tempDuty = period ? tduty - d : d;
          }
        } else {
          p->tempDuty = period ? p->pwmDuty : 0;
        }
      } else {
        p->tempDuty = p->pwmDuty;
      }
#ifndef CPIN_DISABLE_HWTIMER
      if (p->isHW) {
        if (p->prevDuty != p->tempDuty) {
          p->prevDuty = p->tempDuty;
          const uint32_t hw = p->tempDuty > 255 ? 255 : p->tempDuty;
          pwm_start(digitalPinToPinName(p->pinHW), p->pinFrequency,
                    p->lowState == 0 ? hw : 255 - hw, (TimerCompareFormat_t)8);
        }
      } else
#endif
      {
        if (p->tempDuty)
          p->set();
        else
          p->reset();
      }
    } else if (!p->isHW && ((uint32_t)(p->tempDuty + 1) == counter)) {
      p->reset();
    }
  }
}
#ifndef CPIN_DISABLE_HWTIMER
void cPins::initTimer(TIM_TypeDef *timer, uint32_t freq) {
  if (timerInited) {
    freeTimer();
  }
  timerFreq = freq;
  subTicks = 0;

  for (cPins *p : instances) {
    if (p->isHW) {
      PinName pn = digitalPinToPinName(p->pinHW);
      if (timer == (TIM_TypeDef *)pinmap_peripheral(pn, PinMap_PWM)) {
        p->isHW = 0; // timer is busy by us, fall back to software pwm
      }
    }
  }

  T = new HardwareTimer(timer);
  T->pause();
  T->setOverflow(timerFreq * ticksPerCycle, HERTZ_FORMAT);
  T->attachInterrupt(timerCallback);
  timerInited = true;
  T->resume();
}
void cPins::freeTimer(void) {
  if (!T)
    return;
  T->pause();
  delete T;
  T = nullptr;
  timerInited = false;
}
#endif
cPins::~cPins() {
  {
    CritSec cs;
    auto it = std::find(instances.begin(), instances.end(), this);
    if (it != instances.end())
      instances.erase(it);
    it = std::find(localinstances.begin(), localinstances.end(), this);
    if (it != localinstances.end())
      localinstances.erase(it);
    blinkTime = 0;
  }
#ifndef CPIN_DISABLE_HWTIMER
  if (isHW && prevDuty != 0xFFFF)
    pwm_stop(digitalPinToPinName(pinHW));
#endif
  reset();
#ifndef CPIN_DISABLE_HWTIMER
  if (instances.empty() && timerInited)
    freeTimer(); // after last pin deletion free the timer
#endif
}
#ifndef CPIN_DISABLE_HWTIMER
void cPins::setFrequency(uint32_t frequency) {
  if (isHW) {
    pinFrequency = frequency;
    prevDuty = 0xFFFF; // force pwm reconfigure on next cycle
  }
}
#endif
