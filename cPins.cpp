#include <Arduino.h>
#include <HardwareTimer.h>

#include "cPins.h"

uint16_t cPins::pinCounter = 0;
uint64_t cPins::timerCounter = 0;
uint32_t cPins::timerFreq = 0;
uint32_t cPins::localCounter = 0;
uint32_t cPins::prevms = 0;
uint8_t cPins::ledBrightness = 100;
HardwareTimer *cPins::T = nullptr;
bool cPins::timerInited = false;
cPins **cPins::pinsList;
cPins **cPins::localList;

void cPins::pushInstance(cPins *instance)
{
  pinsList = (cPins **)realloc((void *)pinsList, (pinCounter + 1) * sizeof(instance));
  localList = (cPins **)realloc((void *)localList, (pinCounter + 1) * sizeof(instance));
  pinsList[pinCounter++] = instance;
}

cPins::cPins(const char *cname, uint16_t _PIN, uint8_t isled, uint8_t hs)
    : port(get_GPIO_Port(STM_PORT(digitalPin[_PIN]))),
      pin(STM_PIN(digitalPin[_PIN])), pinHW(_PIN), isHW(isled & 2U),
      highState(hs & 0x1), lowState(!(hs & 1U)), isLed(isled & 1U)
{
  uint8_t isOD = hs & 2U;
  pinMode(_PIN, isOD ? OUTPUT_OPEN_DRAIN : OUTPUT);
  if ((isHW) && !(pin_in_pinmap(digitalPinToPinName(_PIN), PinMap_PWM)))
  {
    isHW = 0;
  }
  reset();
  name = (char *)malloc(strlen(cname) + 1);
  strcpy(name, cname);
  if (pinCounter < CPINS_MAX)
  {
    pushInstance(this);
  }
  setPWM(PWMMAX);
}
inline void cPins::write(uint8_t value)
{
  port->BSRR = (1U << pin) << (!(value & 1U) << 4);
}
inline uint8_t cPins::read(void) { return port->IDR & (1U << pin); }
inline void cPins::reset(void) { write(lowState); }
inline void cPins::set(void) { write(highState); }
inline void cPins::toggle(void) { port->ODR ^= (1U << pin); }
bool cPins::isActive(void) { return (blinkTime == 0); }
void cPins::setTime(uint32_t time)
{
  bool timerPaused = false;

  if (timerInited)
  { // threadsafe, don't bother blinkTime till we changing
    // it here
    T->pause();
    timerPaused = true;
  }
  if (!blinkTime)
  { // if pin finished any job already, just init it as NORMAL
    // and reset
    blinkInitTime = blinkTime = time;
    phaseShift = 0;
  }
  else
  {
    switch (mode)
    {
    default:
    case CPIN_NORMAL:
      blinkTime = blinkInitTime = time;
      phaseShift = 0;
      break;
    case CPIN_ADDITIVE:                 // simply add some extra time to it
      if (blinkInitTime + time <= ~0UL) // check for overflow
        blinkTime += time;
      else
        blinkTime = ~0UL;
      if (blinkTime + time <= ~0UL) // same check here
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
  if (timerPaused) // switch timer on back
    T->resume();
}
void cPins::noInterrupt(void) { noInterruptable = true; }
void cPins::setMode(uint8_t newMode)
{
  if (newMode < CPIN_MODEEND)
    mode = newMode;
}

void cPins::setPWM(uint16_t duty)
{
  if (duty <= PWMMAX)
  {
    Duty = duty;
    pwmDuty = isLed ? pwmLED[((ledBrightness * (Duty << 8)) / 100) >> 8]
                    : Duty; // magic duty calcs for led :)
  }
}
void cPins::blink(uint32_t time, uint32_t period)
{
  if (noInterruptable && blinkTime)
    return;
  else
    noInterruptable = false;
  setTime(time);
  blinkPeriod = period;
  toBreathe = 0;
}
void cPins::breathe(uint32_t time, uint32_t period)
{
  if (noInterruptable && blinkTime)
    return;
  else
    noInterruptable = false;
  setTime(time);
  blinkPeriod = period ? period : time;
  toBreathe = 1;
  blinkFade = 0;
}
void cPins::blinkfade(uint32_t time, uint32_t period)
{
  if (noInterruptable && blinkTime)
    return;
  else
    noInterruptable = false;
  setTime(time);
  blinkPeriod = period ? period : time;
  toBreathe = 1;
  blinkFade = 1;
}
void cPins::on(uint32_t time)
{
  if (noInterruptable && blinkTime)
    return;
  else
    noInterruptable = false;
  setTime(time);
  blinkPeriod = 0;
  toBreathe = 0;
}
void cPins::off()
{
  if (noInterruptable && blinkTime)
    return;
  else
    noInterruptable = false;
  blinkTime = blinkInitTime = 0;
  blinkPeriod = 0;
  toBreathe = 0;
  blinkFade = 0;
}
void cPins::on()
{
  if (noInterruptable && blinkTime)
    return;
  else
    noInterruptable = false;
  blinkTime = ~0UL;
  blinkPeriod = 0;
  toBreathe = 0;
  blinkFade = 0;
}
uint16_t cPins::getDuty(void) { return (Duty); }

void cPins::setBrightness(uint8_t brightness)
{
  if (brightness > 100)
    ledBrightness = 100;
  else
    ledBrightness = brightness;

  for (uint16_t cpin = 0; cpin < pinCounter; cpin++)
  {
    if (pinsList[cpin]->isLed == CPIN_LED)
      pinsList[cpin]->setPWM(pinsList[cpin]->getDuty());
  }
}
void cPins::timerCallback(HardwareTimer *ht)
{
  uint32_t counter = timerCounter % ticksPerCycle;
  uint32_t ms = (((timerCounter * 1000)) / (timerFreq * ticksPerCycle));
  if (ms > prevms)
  {
    localCounter = 0;
    for (uint32_t c = 0; c < pinCounter; c++)
    {
      if (pinsList[c]->blinkTime == 0)
      {
        if ((pinsList[c]->isHW) && (pinsList[c]->prevDuty > 0))
        {
          pwm_stop(digitalPinToPinName(pinsList[c]->pinHW));
          pinsList[c]->prevDuty = 0;
        }
        pinsList[c]->reset();
      }
      else
      {
        if (pinsList[c]->blinkTime > ms - prevms) {
          pinsList[c]->blinkTime-= ms - prevms;
        } else {
          pinsList[c]->blinkTime = 0;
        }
        localList[localCounter++] = pinsList[c];     
      }
    }
    prevms = ms;
        
  }
  for (uint16_t c = 0; c < localCounter; c++)
  {
    if (!counter)
    {
      if (localList[c]->blinkPeriod)
      {
        uint32_t period =
            ((localList[c]->blinkInitTime - localList[c]->blinkTime + 1 +
              localList[c]->phaseShift) /
             (localList[c]->blinkPeriod >> 1)) &
            1UL;
        if (localList[c]->toBreathe)
        {
          uint32_t _tduty;
          if (localList[c]->isLed)
          {
            _tduty = pwmLED[((ledBrightness * (localList[c]->getDuty() << 8)) / 100) >> 8];
          }
          else
          {
            _tduty = localList[c]->pwmDuty;
          }
          uint32_t step =
              ((localList[c]->blinkInitTime - localList[c]->blinkTime + 1 +
                localList[c]->phaseShift) %
               (localList[c]->blinkPeriod >> 1));
          if ((!period) && (localList[c]->blinkFade))
          {
            localList[c]->tempDuty = _tduty;
          }
          else
          {
            uint32_t tempDuty = (((_tduty << 8) / (localList[c]->blinkPeriod >> 1)) * step) >> 8;
            localList[c]->tempDuty = period ? _tduty - tempDuty : tempDuty;
          }
        }
        else
        {
          localList[c]->tempDuty = period ? localList[c]->pwmDuty : 0;
        }
      }
      else
      {
        localList[c]->tempDuty = localList[c]->pwmDuty;
      }
      if (localList[c]->isHW)
      {
        if (localList[c]->prevDuty != localList[c]->tempDuty)
        {
          localList[c]->prevDuty = localList[c]->tempDuty;
          pwm_start(digitalPinToPinName(localList[c]->pinHW), CPIN_HWFREQ,
                    localList[c]->lowState == 0 ? localList[c]->tempDuty
                                                : 255 - localList[c]->tempDuty,
                    (TimerCompareFormat_t)8);
        }
      }
      else
      {
        if (localList[c]->tempDuty)
        {
          localList[c]->set();
        }
        else
        {
          localList[c]->reset();
        }
      }
    }
    else
    {
      if ((!localList[c]->isHW) && (localList[c]->blinkTime) &&
          ((uint32_t)(localList[c]->tempDuty + 1) == counter))
      {
        localList[c]->reset();
      }
    }
  }
  timerCounter++;
}
void cPins::initTimer(TIM_TypeDef *timer, uint32_t freq)
{
  timerFreq = freq;
  if (timerInited)
  {
    freeTimer();
  }

  for (uint16_t c = 0; c < pinCounter; c++)
  {
    if (pinsList[c]->isHW == 1)
    {
      PinName p = digitalPinToPinName(pinsList[c]->pinHW);
      if (timer == (TIM_TypeDef *)pinmap_peripheral(p, PinMap_PWM))
      {
        pinsList[c]->isHW = 0;
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
void cPins::freeTimer(void)
{
  T->pause();
  delete T;
  timerInited = false;
  timerFreq = 0;
}
cPins::~cPins()
{
  pinCounter--;
  if (pinCounter)
  {
    pinsList = (cPins **)realloc((void *)pinsList, pinCounter * sizeof(cPins *));
    localList = (cPins **)realloc((void *)localList, pinCounter * sizeof(cPins *));
  }
  if (!pinCounter && timerInited)
    freeTimer(); // normally it should never happen cuz all pins have to be
                 // statically defined, but if you want to malloc cpins
                 // instatnces, it will free timer after last pin deletion
  if (name != nullptr)
    free(name);
}