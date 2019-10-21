/******************************/
/* Non blocking blink example */
/******************************/

// Prepare suitable timer
#if defined(TIM3)
#define TIMx TIM3
#elif defined(TIM2)
#define TIMx TIM2
#else
#define TIMx TIM1
#endif

#include <cPins.h>

CPIN(LED, LED_BUILTIN, CPIN_LED,
     LOW); // define LED_BUILTIN pin as LED and _ACTIVE LOW_

void setup() {
  // put your setup code here, to run once:
  cPins::initTimer(TIMx);
}
void loop() {
  // put your main code here, to run repeatedly:
  LED.blink(~0UL, 2000); // blink 2 secs, 1/2 (1s) HIGH, 1/2 LOW
  while (1) {
    /* do stuff */
  }
}
