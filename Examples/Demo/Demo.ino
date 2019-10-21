/************************/
/* cPins functions DEMO */
/************************/

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
  LED.blink(10000, 2000); // blink 2 secs, 1/2 (1s) HIGH, 1/2 LOW
  delay(10000);

  LED.blinkfade(10000, 2000); // blink with fadeout
  delay(10000);

  LED.breathe(10000, 2000); // breathe
  delay(10000);

  LED.on(2000); // just on for 2s
  for (uint8_t brightness = 255; brightness > 0; brightness -= 5) {
    LED.setPWM(brightness); // decrease brightness
    delay(200);
  }
  LED.setPWM(255); // get brightness back to max
  delay(1000);

  LED.on(); // infinite on
  delay(3000);

  LED.off(); // off
  delay(3000);
}
