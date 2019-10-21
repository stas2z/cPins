# cPins

Simple and easy to use timer based pins/leds switching on/off/blink/PWM library for STM32. Using HardwareTimer class comes since core version 1.7.0.

## Usage

Include library and define your pins as pins or leds

```
#include <cPins.h>

CPIN(PIN_PA0, PA0 /*, CPIN_PIN, HIGH*/); // usual pin assigned to PA0 (active HIGH)
CPIN(PIN_PA1, PA1, CPIN_PIN, LOW); //same but active LOW
CPIN(PIN_PA2, PA2, CPIN_LED, HIGH); //define PA2 as LED active HIGH
CPIN(LED, LED_BUILTIN, CPIN_LED /*, LOW*/); // define LED_BUILTIN as LED active LOW
```

Then you need to assign timer for library:

```
void setup() {
...
  cPins::initTimer(); // Will use TIM1 and 60Hz frequency by default
...
}
```

If you need to specify timer and frequency, use full call:
```
void setup() {
...
  cPins::initTimer(TIM2, 90); // Will use TIM2 and 90Hz frequency by default
...
}
```

Thats all, you can use your pins/leds by calling em:
```
LED.blink(2000, 1000); // 2s long, 2s for a full cycle, 1s for ON and 1s for OFF states
PIN_PA0.on(); // just switch pin ON
PIN_PA0.setPWM(100); // set PWM duty for pin, 8bit PWM implemented, so duty can be set to 0..255
PIN_PA1.blinkfade(10000, 1000); // blink with fadeout
PIN_PA2.breathe(4000,2000); // breathe (soft fade in/fade out)
```