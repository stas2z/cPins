#include <wiring.h>

#include "stm32_def.h"
#include "stm32yyxx_ll_gpio.h"

#include <vector>

#ifndef CPIN_DISABLE_HWTIMER
#include <HardwareTimer.h>
#endif

#include "cPins.h"

uint64_t cPins::timerCounter = 0;
uint32_t cPins::timerFreq = 1;
uint32_t cPins::prevms = 0;
uint8_t cPins::ledBrightness = 100;
#ifndef CPIN_DISABLE_HWTIMER
HardwareTimer *cPins::T = nullptr;
bool cPins::timerInited = false;
#endif

cPins::cPins(const char *cname, uint16_t _PIN, uint8_t isled, uint8_t hs)
    : port(get_GPIO_Port(STM_PORT(digitalPinToPinName(_PIN)))),
      pin(STM_GPIO_PIN(digitalPinToPinName(_PIN))), pinHW(_PIN),
      isHW(isled & 2U), highState(hs & 1U), lowState(!(hs & 1U)),
      isLed(isled & 1U) {
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
  name = (char *)malloc(strlen(cname) + 1);
  strcpy(name, cname);
  instances.push_back(this);
  _me = instances.end();
  setPWM(PWMMAX);
}
inline void cPins::write(uint8_t value) {
  port->BSRR = (pin) << (!(value & 1U) << 4);
}
// only to read output ports, input ports should be read from IDR
inline uint8_t cPins::read(void) { return port->ODR & (pin); }
inline void cPins::reset(void) { write(lowState); }
inline void cPins::set(void) { write(highState); }
inline void cPins::toggle(void) { port->ODR ^= (pin); }
bool cPins::isActive(void) { return (blinkTime == 0); }
uint32_t cPins::getRemaining(void) { return blinkTime; }
uint32_t cPins::getTime(void) {
  // if (isActive())
  return blinkInitTime;
  // else
  //  return 0;
}
void cPins::setTime(uint32_t time) {
#ifndef CPIN_DISABLE_HWTIMER
  bool timerPaused = false;

  if (timerInited) { // threadsafe, don't bother blinkTime till we changing
    // it here
    T->pause();
    timerPaused = true;
  }
#endif
  if (!blinkTime) { // if pin finished any job already, just init it as NORMAL
    // and reset
    blinkInitTime = blinkTime = time;
    phaseShift = 0;
  } else {
    switch (mode) {
    default:
    case CPIN_NORMAL:
      blinkTime = blinkInitTime = time;
      phaseShift = 0;
      break;
    case CPIN_ADDITIVE:             // simply add some extra time to it
      if (blinkTime + time <= ~0UL) // check for overflow
        blinkTime += time;
      else
        blinkTime = ~0UL;
      if (blinkInitTime + time <= ~0UL) // same check here
        blinkInitTime += time;
      else
        blinkInitTime = ~0UL;
      break;
    case CPIN_CONTINUE:
      if (blinkPeriod) // calculate the shift of phase to continue at the same
                       // state
        phaseShift = (blinkInitTime - blinkTime + phaseShift) % (blinkPeriod);
      else
        phaseShift = 0;
      blinkInitTime = blinkTime = time;
      break;
    }
  }
#ifndef CPIN_DISABLE_HWTIMER
  if (timerPaused) // switch timer on back
    T->resume();
#endif
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
void cPins::blink(uint32_t time, uint32_t period) {
  if (noInterruptable && blinkTime)
    return;
  else
    noInterruptable = false;
  setTime(time);
  blinkPeriod = period;
  toBreathe = 0;
}
void cPins::breathe(uint32_t time, uint32_t period) {
  if (noInterruptable && blinkTime)
    return;
  else
    noInterruptable = false;
  setTime(time);
  blinkPeriod = period ? period : time;
  toBreathe = 1;
  blinkFade = 0;
}
void cPins::blinkfade(uint32_t time, uint32_t period) {
  if (noInterruptable && blinkTime)
    return;
  else
    noInterruptable = false;
  setTime(time);
  blinkPeriod = period ? period : time;
  toBreathe = 1;
  blinkFade = 1;
}
void cPins::on(uint32_t time) {
  if (noInterruptable && blinkTime)
    return;
  else
    noInterruptable = false;
  setTime(time);
  blinkPeriod = 0;
  toBreathe = 0;
}
void cPins::off() {
  if (noInterruptable && blinkTime)
    return;
  else
    noInterruptable = false;
  finishCallback(this); // call callback at the end
  blinkTime = 0;
  blinkPeriod = 0;
  toBreathe = 0;
  blinkFade = 0;
  blinkInitTime = 0;
}
uint16_t cPins::getDuty(void) { return (Duty); }

void cPins::setBrightness(uint8_t brightness) {
  if (brightness > 100)
    ledBrightness = 100;
  else
    ledBrightness = brightness;

  for (uint16_t cpin = 0; cpin < instances.size(); cpin++) {
    if (instances[cpin]->isLed == CPIN_LED)
      instances[cpin]->setPWM(instances[cpin]->getDuty());
  }
}

void cPins::timerCallback(void) {
  uint32_t counter = timerCounter % ticksPerCycle;
  uint32_t ms = (((timerCounter * 1000)) / (timerFreq * ticksPerCycle));
  if (ms > prevms) {
    std::vector<cPins *> oldinstances = localinstances;
    localinstances.clear();
    for (uint32_t c = 0; c < instances.size(); c++) {
      if (instances[c]->blinkTime == 0) {
        if ((instances[c]->isHW) && (instances[c]->prevDuty > 0)) {
#ifndef CPIN_DISABLE_HWTIMER
          pwm_start(digitalPinToPinName(instances[c]->pinHW),
                    instances[c]->pinFrequency, 0, (TimerCompareFormat_t)8);
#endif
          instances[c]->prevDuty = 0;
        }
        for (uint32_t old = 0; old < oldinstances.size(); old++) {
          if (instances[c] == oldinstances[old]) {
            instances[c]->finishCallback(
                instances[c]); // call callback at the end
            instances[c]->reset();
            break;
          }
        }
        if (instances[c]->blinkInitTime > 0) {
          instances[c]->blinkInitTime = 0;
        }
      } else {
        if (instances[c]->blinkTime > ms - prevms) {
          instances[c]->blinkTime -= ms - prevms;
        } else {
          instances[c]->blinkTime = 0;
        }
        localinstances.push_back(instances[c]);
      }
      instances[c]->tickCallback(instances[c]); // call every tick callback
    }
    prevms = ms;
  }
  for (uint16_t c = 0; c < localinstances.size(); c++) {
    if (!counter) {
      if (localinstances[c]->blinkPeriod) {
        uint32_t period =
            ((localinstances[c]->blinkInitTime - localinstances[c]->blinkTime +
              1 + localinstances[c]->phaseShift) /
             (localinstances[c]->blinkPeriod >> 1)) &
            1UL;
        if (localinstances[c]->toBreathe) {
          uint32_t _tduty;
          if (localinstances[c]->isLed) {
            _tduty =
                pwmLED[((ledBrightness * (localinstances[c]->getDuty() << 8)) /
                        100) >>
                       8];
          } else {
            _tduty = localinstances[c]->pwmDuty;
          }
          uint32_t step = ((localinstances[c]->blinkInitTime -
                            localinstances[c]->blinkTime + 1 +
                            localinstances[c]->phaseShift) %
                           (localinstances[c]->blinkPeriod >> 1));
          if ((!period) && (localinstances[c]->blinkFade)) {
            localinstances[c]->tempDuty = _tduty;
          } else {
            uint32_t tempDuty =
                (((_tduty << 8) / (localinstances[c]->blinkPeriod >> 1)) *
                 step) >>
                8;
            localinstances[c]->tempDuty = period ? _tduty - tempDuty : tempDuty;
          }
        } else {
          localinstances[c]->tempDuty = period ? localinstances[c]->pwmDuty : 0;
        }
      } else {
        localinstances[c]->tempDuty = localinstances[c]->pwmDuty;
      }
#ifndef CPIN_DISABLE_HWTIMER
      if (localinstances[c]->isHW) {
        if (localinstances[c]->prevDuty != localinstances[c]->tempDuty) {
          localinstances[c]->prevDuty = localinstances[c]->tempDuty;
          pwm_start(digitalPinToPinName(localinstances[c]->pinHW),
                    localinstances[c]->pinFrequency,
                    localinstances[c]->lowState == 0
                        ? localinstances[c]->tempDuty
                        : 255 - localinstances[c]->tempDuty,
                    (TimerCompareFormat_t)8);
        }
      } else
#endif
      {
        if (localinstances[c]->tempDuty) {
          localinstances[c]->set();
        } else {
          localinstances[c]->reset();
        }
      }
    } else {
      if ((!localinstances[c]->isHW) && (localinstances[c]->blinkTime) &&
          ((uint32_t)(localinstances[c]->tempDuty + 1) == counter)) {
        localinstances[c]->reset();
      }
    }
  }
  timerCounter++;
}
#ifndef CPIN_DISABLE_HWTIMER
void cPins::initTimer(TIM_TypeDef *timer, uint32_t freq) {
  timerFreq = freq;
  if (timerInited) {
    freeTimer();
  }

  for (uint16_t c = 0; c < instances.size(); c++) {
    if (instances[c]->isHW == 1) {
      PinName p = digitalPinToPinName(instances[c]->pinHW);
      if (timer == (TIM_TypeDef *)pinmap_peripheral(p, PinMap_PWM)) {
        instances[c]->isHW = 0;
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
  T->pause();
  delete T;
  timerInited = false;
  timerFreq = 0;
}
#endif
cPins::~cPins() {
  if (name != nullptr)
    free(name);
  instances.erase(_me);
#ifndef CPIN_DISABLE_HWTIMER
  if (instances.empty() && timerInited)
    freeTimer(); // normally it should never happen cuz all pins have to be
// statically defined, but if you want to malloc cpins
// instances, it will free timer after last pin deletion
#endif
}
#ifndef CPIN_DISABLE_HWTIMER
void cPins::setFrequency(uint32_t frequency) {
  if (isHW) {
    pinFrequency = frequency;
  }
}
#endif