#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_MCP4728.h>
#include <Arduino.h>

#define LED_PIN 1 // PD1 (Pin 31 on ATmega328)
#define NUM_LEDS 8

#define MUX_S0 2 // PD2
#define MUX_S1 3 // PD3
#define MUX_S2 4 // PD4
#define MUX_SIG0 A0 // PC0
#define MUX_SIG1 A1 // PC1
#define MUX_SIG2 A2 // PC2
#define BUTTON_PIN A3 // PC3

#define CLOCK_IN 9 // PB1
#define RESET_IN_A 8 // PB0
#define RESET_IN_B 10 // PB2
#define GATE_OUT1 5 // PD5
#define GATE_OUT2 6 // PD6
#define GATE_OUT3 7 // PD7


Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_MCP4728 dac;

volatile bool clockState = LOW;
volatile bool lastClockState = LOW;
volatile bool ResetAState = LOW;
volatile bool lastResetAState = LOW;
volatile bool ResetBState = LOW;
volatile bool lastResetBState = LOW;
int step1 = 0;
int step2 = 0;

int sequenceA[NUM_LEDS];
int sequenceB[NUM_LEDS];
int divSeqA = 1;
int divSeqB = 1;
int probSeq = 50;
int cvDivA = 0;
int cvDivB = 0;
int cvProb = 0;
int scalingFactorA = 1023;
int scalingFactorB = 1023;
int scale = 0;
int now = 0;

const int GATE_WIDTH = 5000;

void setup() {
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  startupAnimation(); // Play the startup animation

  // Initialize GPIOs
  pinMode(CLOCK_IN, INPUT);
  pinMode(GATE_OUT1, OUTPUT);
  pinMode(GATE_OUT2, OUTPUT);
  pinMode(GATE_OUT3, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);

  // Initialize DAC (MCP4728)
  if (!dac.begin()) {
    while (1); // DAC initialization failed
  }

  // Initialize sequences to zero
  memset(sequenceA, 0, sizeof(sequenceA));
  memset(sequenceB, 0, sizeof(sequenceB));
}

void loop() {
  clockState = digitalRead(CLOCK_IN);
  ResetAState = digitalRead(RESET_IN_A);
  ResetBState = digitalRead(RESET_IN_B);

  if (lastResetAState == LOW && ResetAState == HIGH) {
    step1 = 0;
  }

  if (lastResetBState == LOW && ResetBState == HIGH) {
    step2 = 0;
  }

  if (lastClockState == LOW && clockState == HIGH) {
    handleClockPulse();
  }

  // Update controls
  readControls();

  if (digitalRead(BUTTON_PIN) == LOW) {
    if(hasPotMoved(0)) {
      updateScalingVisualization(scalingFactorA, 200, 20, 0);
    } else if (hasPotMoved(1)) {
      updateScalingVisualization(scalingFactorB, 0, 200, 200);
    } else if (hasPotMoved(2)) {
      colorStrip(scale);
    }
  }

  

  if (micros() - now > GATE_WIDTH) {
    digitalWrite(GATE_OUT1, LOW);
    digitalWrite(GATE_OUT2, LOW);
    digitalWrite(GATE_OUT3, LOW);
  }

  lastClockState = clockState;
}

void handleClockPulse() {
  now = micros();
  static int counterA = 0, counterB = 0;

  // Advance sequence steps based on clock division
  if (++counterA >= divSeqA) {
    counterA = 0;
    step1 = (step1 + 1) % NUM_LEDS;
  }

  if (++counterB >= divSeqB) {
    counterB = 0;
    step2 = (step2 + 1) % NUM_LEDS;
  }

  // Update LEDs with sequences
  updateLEDs();

  // Output sequence values to DAC
  outputSequenceToDAC();

  // Set gate outputs based on conditions
  if (sequenceA[step1] > sequenceB[step2]) {
    if (counterA == 0) {
      digitalWrite(GATE_OUT1, HIGH);
    }
  } else {
    if (counterB == 0) {
      digitalWrite(GATE_OUT1, HIGH);
    }
  }

  if (counterA == 0) {
    digitalWrite(GATE_OUT2, HIGH);
  }

  if (counterB == 0) {
    digitalWrite(GATE_OUT3, HIGH);
  }
}

void updateLEDs() {
  if (!(digitalRead(BUTTON_PIN) == LOW)) {
    strip.clear();
    for (int i = 0; i < NUM_LEDS; i++) {
      if (i == step1 && i == step2) {
        // If both sequences are on the same step, mix colors
        strip.setPixelColor(i, strip.Color(128, 0, 128)); // Purple as a mix of Sequence 1 and 2 colors
      } else if (i == step1) {
        // Sequence 1: Cyan
        strip.setPixelColor(i, strip.Color(0, 200, 200));
      } else if (i == step2) {
        // Sequence 2: Red
        strip.setPixelColor(i, strip.Color(200, 20, 0));
      }
    }
  }
  strip.show();
}

void readControls() {
  // Read next pots for sequences A and B
  int nextStepA = (step1 + 1) % NUM_LEDS;
  int nextStepB = (step2 + 1) % NUM_LEDS;
  sequenceA[nextStepA] = readMux(MUX_SIG0, nextStepA); //* (scalingFactorA / 255);
  sequenceB[nextStepB] = readMux(MUX_SIG1, nextStepB); //* (scalingFactorB / 255);
  cvProb = readMux(MUX_SIG2, 5) * 1.7 - 511;
  cvDivA = readMux(MUX_SIG2, 3) * 1.7 - 511;
  cvDivB = readMux(MUX_SIG2, 4) * 1.7 - 511;
  
  if(hasPotMoved(0)) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      scalingFactorA = readMux(MUX_SIG2, 0);
    } else {
      divSeqA = mapToDivisions((readMux(MUX_SIG2, 0) + cvDivA));
    }
  } else if (hasPotMoved(1)) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      scalingFactorB = readMux(MUX_SIG2, 1);
    } else {
      divSeqB = mapToDivisions((readMux(MUX_SIG2, 1) + cvDivB));
    }
  } else if (hasPotMoved(2)) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      scale = map(readMux(MUX_SIG2, 2), 0, 1023, 0, 6);
    } else {
      probSeq = map(readMux(MUX_SIG2, 2) + cvProb, 0, 1023, 0, 100);
    }
  }
  
}

int mapToDivisions(int value) {
  if (value < 170) return 1;
  if (value < 340) return 2;
  if (value < 510) return 3;
  if (value < 680) return 4;
  if (value < 850) return 8;
  return 16;
}

int readMux(int sigPin, int channel) {
  digitalWrite(MUX_S0, bitRead(channel, 0));
  digitalWrite(MUX_S1, bitRead(channel, 1));
  digitalWrite(MUX_S2, bitRead(channel, 2));
  delayMicroseconds(50); // Allow settling time
  return analogRead(sigPin);
}

void outputSequenceToDAC() {
  int seq_a_out = quantize(sequenceA[step1] * (scalingFactorA / 1023));
  int seq_b_out = quantize(sequenceB[step2] * (scalingFactorB / 1023));
  dac.setChannelValue(MCP4728_CHANNEL_A, seq_a_out, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  dac.setChannelValue(MCP4728_CHANNEL_B, seq_b_out, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  int mixedValue = (seq_a_out + seq_b_out) / 2;
  dac.setChannelValue(MCP4728_CHANNEL_C, mixedValue, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  if (random(0, 100) < probSeq) {
    dac.setChannelValue(MCP4728_CHANNEL_D, seq_a_out, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  } else {
    dac.setChannelValue(MCP4728_CHANNEL_D, seq_b_out, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  }
}

void updateScalingVisualization(int value, int r, int g, int b) {
  strip.clear();
  
  // Map the value to determine the number of lit LEDs
  float scaledValue = map(value, 0, 980, 0, NUM_LEDS * 100) / 100.0; // Scaled to have fractional precision
  int fullLitLEDs = (int)scaledValue; // Integer part: fully lit LEDs
  float fractionalPart = scaledValue - fullLitLEDs; // Fractional part: brightness for the last LED
  
  // Light up fully lit LEDs
  for (int i = 0; i < fullLitLEDs; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }

  // Adjust brightness of the next LED based on fractional part
  if (fullLitLEDs < NUM_LEDS) {
    int dimmedR = (int)(r * fractionalPart);
    int dimmedG = (int)(g * fractionalPart);
    int dimmedB = (int)(b * fractionalPart);
    strip.setPixelColor(fullLitLEDs, strip.Color(dimmedR, dimmedG, dimmedB));
  }

  strip.show();
}

void colorStrip(int color) {
  uint8_t r = 0, g = 0, b = 0;

  switch (color) {
    case 0: r = 255; g = 0; b = 0; break; // Red
    case 1: r = 0; g = 255; b = 0; break; // Green
    case 2: r = 0; g = 0; b = 255; break; // Blue
    case 3: r = 255; g = 255; b = 0; break; // Yellow
    case 4: r = 0; g = 255; b = 255; break; // Cyan
    case 5: r = 255; g = 0; b = 255; break; // Magenta
    case 6: r = 255; g = 255; b = 255; break; // White
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }

  strip.show();
}


void startupAnimation() {
  // Pulsing animation with RGB wavy effect, max duration ~3 seconds
  for (int pulse = 0; pulse < 3; pulse++) { // 6 pulses, ~3 seconds
    for (int intensity = 0; intensity <= 255; intensity += 15) {
      for (int i = 0; i < strip.numPixels(); i++) {
        int wave = (intensity + i * 30) % 255; // Create a wavy effect
        strip.setPixelColor(i, strip.Color(wave, 255 - wave, wave / 2));
      }
      strip.show();
      delay(20);
    }
    for (int intensity = 255; intensity >= 0; intensity -= 15) {
      for (int i = 0; i < strip.numPixels(); i++) {
        int wave = (intensity + i * 30) % 255; // Create a wavy effect
        strip.setPixelColor(i, strip.Color(wave, 255 - wave, wave / 2));
      }
      strip.show();
      delay(20);
    }
  }
}

bool hasPotMoved(int muxChannel) {
  int firstValue = readMux(MUX_SIG2, muxChannel);
  delay(5);
  int secondValue = readMux(MUX_SIG2, muxChannel);

  return abs(secondValue - firstValue) > 3; // Threshold for movement detection
}


int quantize(int note) {
  int semitone = note / 36;
  return 200 + (semitone * 83.3);
}
