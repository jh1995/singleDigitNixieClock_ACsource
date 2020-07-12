#include <Entropy.h>
#include <digitalWriteFast.h>

#define ANODEPIN 5
#define BCD1PIN 16
#define BCD2PIN 13
#define BCD3PIN 14
#define BCD4PIN 15
#define LPPIN 19
#define RPPIN 18
#define BUTTONPIN 7


#define PWMOFF 0
#define PWMON 255

#define DONTBLINK 1
#define DOBLINK   0


// pin for timekeeping, D3
#define fiftyHzInterruptPin 3

int tickElapsed = 0;
//int oldTickElapsed = 0;
int secondsElapsed = 0;
int oldSecondsElapsed = 0;
int blinkState;
unsigned int pwmLevel = 0;
unsigned int fadeSpeed = 30;
unsigned long millisOld = 0;
const long millisDiff = 19;

// start time, until we implement a contactless way to set the time 
int hours = 20;
int minutes = 00;

int decoderPins[6] = { BCD1PIN, BCD2PIN, BCD3PIN, BCD4PIN, LPPIN, RPPIN}; //LSB first

// remapping 74141/K155 output
// 0354986721 <- shown in the tube
// 0123456789 <- decoded by the IC
int symbolMap[10] = {0,9,8,1,3,2,6,7,5,4};

// variables needed for debouncing the button
int lastButtonState = HIGH; // 1 = not pressed
int buttonState;
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

int setupMode = 0;

void setup() {

  Serial.begin(9600);

  pinMode(BUTTONPIN, INPUT_PULLUP); // the button closes to ground, so we offer some V support :)
  pinMode(ANODEPIN, OUTPUT);
  pwmLevel = PWMOFF;
  analogWrite (ANODEPIN, pwmLevel); // switch off

  // outputs to 74141 and DP transistors
  pinMode( BCD1PIN, OUTPUT);
  pinMode( BCD2PIN, OUTPUT);
  pinMode( BCD3PIN, OUTPUT);
  pinMode( BCD4PIN, OUTPUT);
  pinMode( LPPIN, OUTPUT);
  pinMode( RPPIN, OUTPUT);


  // PRNG init
  uint32_t seed_value;
  Entropy.initialize();
  seed_value = Entropy.random();
  randomSeed(seed_value);  

  if (digitalRead(BUTTONPIN) == LOW) { // fine, skipping debouncing
    setupMode = 1;
    analogWrite (ANODEPIN, PWMON);
    Serial.println("Entering setup mode ...");
  }

  // the external timebase
  pinMode( fiftyHzInterruptPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(fiftyHzInterruptPin), fiftyHzISR, FALLING); 

}


int readButton() {
   
  // read the state of the switch into a local variable:
  int reading = digitalRead(BUTTONPIN);

  // check to see if you just pressed the button
  // (i.e. the input went from HIGH to LOW), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;

      // only toggle the LED if the new button state is HIGH
      if (buttonState == LOW) {
        return 1; // return putton pressed
      } else {
        return 0; // return button depressed
      }
    }
  }
} // end sub

// ISR triggered every 20 milliseconds or so. It checks that at least 19 ms
// have passed. Not sure what happens when millis overflow and return to zero.
void fiftyHzISR() {

 tickElapsed = tickElapsed + 1;

//  unsigned long currentMillis = millis();
//
//  if (currentMillis - millisOld > millisDiff) {
//    // 50 ticks make one second, handled in the main loop
//    tickElapsed = tickElapsed + 1;
//    millisOld = currentMillis;
//  }

}



byte decToBcd(byte val)
{
  return ( ((val/10)*16) + (val%10) );
}

void outputToNixie( int digitOut, int lpOut, int rpOut, int dontBlink) {

  if ( digitOut == 16) {
    digitalWriteFast(BCD4PIN, 1);
    digitalWriteFast(BCD3PIN, 1);
  } else {

    int digitOutBCD = decToBcd(symbolMap[digitOut]); 
  
    unsigned int pos = 1;
  
    for(int i=0; i < 4; i++){
      unsigned int mybit = digitOutBCD & pos;
      digitalWrite(decoderPins[i], mybit);
      pos = pos << 1;
    }
  }

    // *** TODO Can be improved
  if (dontBlink != 1) {
    if (blinkState > 0) {
      digitalWrite(decoderPins[4], lpOut);
      digitalWrite(decoderPins[5], rpOut);
    } else {
      digitalWriteFast(LPPIN, 0);
      digitalWriteFast(RPPIN, 0);
    }
  } else {
    digitalWrite(decoderPins[4], lpOut);
    digitalWrite(decoderPins[5], rpOut);
  }

  fadeIn();

}


void outputToNixieNoFade( int digitOut, int lpOut, int rpOut, int dontBlink) {


  if ( digitOut == 16) {
    digitalWriteFast(BCD4PIN, 1);
    digitalWriteFast(BCD3PIN, 1);
  } else {

    int digitOutBCD = decToBcd(symbolMap[digitOut]); 
  
    unsigned int pos = 1;
  
    for(int i=0; i < 4; i++){
      unsigned int mybit = digitOutBCD & pos;
      digitalWrite(decoderPins[i], mybit);
      pos = pos << 1;
    }
  }

  // *** TODO Can be improved
  if (dontBlink != 1) {
    if (blinkState > 0) {
      digitalWrite(decoderPins[4], lpOut);
      digitalWrite(decoderPins[5], rpOut);
    } else {
      digitalWriteFast(LPPIN, 0);
      digitalWriteFast(RPPIN, 0);
    }
  } else {
    digitalWrite(decoderPins[4], lpOut);
    digitalWrite(decoderPins[5], rpOut);
  }

}



// turn off all outputs and switch off PWM
void allOff() {

  outputToNixieNoFade(16,0,0,DOBLINK);

  pwmLevel = PWMOFF;
  analogWrite (ANODEPIN, pwmLevel);
  
}

// increase minutes by one, or reset to zero and call to increase hours
void increaseMinutes() {

  minutes++;

  if (minutes > 59) {
    increaseHours();
    minutes = 0;
  }
  
}

// increase hours, or reset
void increaseHours() {

  hours++;

  if (hours > 23) {
    hours = 0;
  }
  
}

// routine to switch off the nixie by fading out
void fadeOut() {

  // do the fadeOut only if we're not already OFF
  if (pwmLevel != PWMOFF) {
    for (int i=255; i>100; i=i-fadeSpeed) {
      analogWrite (ANODEPIN, i);
      delay(20);
    }
  }

  pwmLevel = PWMOFF;
  analogWrite (ANODEPIN, pwmLevel);

}

// routine to switch off the nixie by fading out
void fadeIn() {

  // do the fadeOut only if we're not already OFF
  if (pwmLevel != PWMON) {
    for (int i=10; i<255; i=i+fadeSpeed) {
      analogWrite (ANODEPIN, i);
      delay(20);
    }
  }

  pwmLevel = PWMON;
  analogWrite (ANODEPIN, pwmLevel);

}

void fastShow(int digitOut, int lpOut, int rpOut, int dontBlink) {

  int maxLoop = digitOut+10;
  int toShow_local;
  
  if (oldSecondsElapsed != secondsElapsed) {
    for (int fsi=0; fsi<maxLoop; fsi++) {
      toShow_local = fsi % 10;
      outputToNixieNoFade(toShow_local,0,0,dontBlink);
      delay(15);
    }
  }
  oldSecondsElapsed = secondsElapsed;

  outputToNixieNoFade(digitOut, lpOut, rpOut, dontBlink);
  
}

int increaseAndShow(int newValueL, int maxValue) {

  int tens=0;
  int twenties=0;
  int toShow;

  Serial.print("increaseAndShow: ");
  
  newValueL++;
  newValueL = newValueL % maxValue;
  toShow = newValueL % 10;

  if (newValueL > 9) {
    tens = 1;
  }

  if (newValueL > 19) {
    twenties = 1;
  }

  Serial.println(toShow);
  
  outputToNixieNoFade(toShow,tens,twenties,1);

  delay(1000);

  return newValueL;
}


// *****************************************************
// **  MAIN   MAIN    MAIN  ****************************
// *****************************************************

void loop() {

  int toShow;
  static int newValue;

  
    // handle blinking
    if (tickElapsed < 25) {
      blinkState = 1;
    } else {
      blinkState = 0;
    }
  
    // handle loop of 1/50s ticks and counts seconds
    if (tickElapsed > 49) {
      oldSecondsElapsed = secondsElapsed;
      secondsElapsed++;
      tickElapsed = 0;
    }
  
    // handle loop of 60 seconds (0..59)
    if (secondsElapsed > 59) {
      increaseMinutes();
      secondsElapsed = 0;
      fadeSpeed = random(1, 11) * 10; // randomize new fading timing
    }
  
  
    // do the display business
    switch (secondsElapsed) {
      case 0:
      case 20:
      case 40:
        toShow = hours / 10;
        outputToNixie(toShow,1,0,DONTBLINK);
        break;
      case 13:
      case 33:
      case 53:
        toShow = hours / 10;
        fastShow(toShow,1,1,DONTBLINK);
        break;
      case 14:
      case 34:
      case 54:
        toShow = hours % 10;
        fastShow(toShow,1,0,DONTBLINK);
        break;
      case 15:
      case 35:
      case 55:
        toShow = minutes / 10;
        fastShow(toShow,0,1,DONTBLINK);
        break;
      case 16:
      case 36:
      case 56:
        toShow = minutes % 10;
        fastShow(toShow,0,0,DONTBLINK);
        break;            
      case 1:
      case 21:
      case 41:
        fadeOut();
        allOff();
        break;
      case 2:
      case 22:
      case 42:
        toShow = hours % 10;
        outputToNixie(toShow,1,0, DONTBLINK);   
        break;
      case 3:
      case 23:
      case 43:
        fadeOut();
        allOff();
        break;
      case 4:
      case 24:
      case 44:
        // allOff(); not needed because the 16 takes care of it:
        outputToNixie(16,1,1,DONTBLINK); // just the two dots
        break;
      case 5:
      case 25:
      case 45:
        fadeOut();
        allOff();
        break;
      case 6:
      case 26:
      case 46:
        toShow = minutes / 10;
        outputToNixie(toShow,0,1,DONTBLINK);    
        break;
      case 7:
      case 27:
      case 47:
        fadeOut();
        allOff();
        break;
      case 8:
      case 28:
      case 48:
        toShow = minutes % 10;
        outputToNixie(toShow,0,1,DONTBLINK);
        break;
      case 9:
      case 29:
      case 49:
        fadeOut();
        allOff();
        break;      
      default:
        outputToNixie(16,1,1,DOBLINK); // show only dots
        break;
      
    }

}


