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
  LED.blink(2000, 2000);      // blink 2 secs, 1/2 (1s) HIGH, 1/2 LOW
  LED.setMode(CPIN_CONTINUE); // make any new call phase friendly
  while (1) {
    LED.blink(2000, 2000);
    // actually it's not a good idea to call it such way without any
    // delay but it's just a demo of phase friendly mode
    // anyway, do stuff
  }
}
