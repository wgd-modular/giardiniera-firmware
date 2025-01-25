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
int divisionPotA = 0;
int divisionPotB = 0;
int divisionA = 1;
int divisionB = 1;
int probSeq = 50;
int probPot = 511;
int cvDivA = 0;
int cvDivB = 0;
int cvProb = 0;
int scalingFactorA = 1023;
int scalingFactorB = 1023;
int scale = 0;
int now = 0;

const int GATE_WIDTH = 10000;

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

  bool p0_moved = hasPotMoved(0);
  bool p1_moved = hasPotMoved(1);
  bool p2_moved = hasPotMoved(2);

  // Update controls
  readControls(p0_moved, p1_moved, p2_moved);

  if (digitalRead(BUTTON_PIN) == LOW) {
    if(p0_moved) {
      updateScalingVisualization(scalingFactorA, 200, 20, 0);
    } else if (p1_moved) {
      updateScalingVisualization(scalingFactorB, 0, 200, 200);
    } else if (p2_moved) {
      colorStrip(scale);
    }
  }

  

  if (micros() - now > GATE_WIDTH) {
    digitalWrite(GATE_OUT1, LOW);
    digitalWrite(GATE_OUT2, LOW);
    digitalWrite(GATE_OUT3, LOW);
  }

  lastClockState = clockState;
  lastResetAState = ResetAState;
  lastResetBState = ResetBState;
}

void handleClockPulse() {
  now = micros();
  static int counterA = 0, counterB = 0;

  // Advance sequence steps based on clock division
  if (++counterA >= divisionA) {
    counterA = 0;
    step1 = (step1 + 1) % NUM_LEDS;
  }

  if (++counterB >= divisionB) {
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

void readControls(bool p0_moved, bool p1_moved, bool p2_moved) {
  // Read next pots for sequences A and B
  int nextStepA = (step1 + 1) % NUM_LEDS;
  int nextStepB = (step2 + 1) % NUM_LEDS;
  sequenceA[nextStepA] = quantize(map(readMux(MUX_SIG0, nextStepA), 0, 1023, 0, scalingFactorA), scale);
  sequenceB[nextStepB] = quantize(map(readMux(MUX_SIG1, nextStepB), 0, 1023, 0, scalingFactorB), scale);
  cvProb = (readMux(MUX_SIG2, 5) - 310) * 6.8;
  cvDivA = (readMux(MUX_SIG2, 3) - 310) * 6.8;
  cvDivB = (readMux(MUX_SIG2, 4) - 310) * 6.8;
  
  if(p0_moved) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      scalingFactorA = readMux(MUX_SIG2, 0);
    } else {
      divisionPotA = readMux(MUX_SIG2, 0);
    }
  } else if (p1_moved) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      scalingFactorB = readMux(MUX_SIG2, 1);
    } else {
      divisionPotB = readMux(MUX_SIG2, 1);
    }
  } else if (p2_moved) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      scale = map(readMux(MUX_SIG2, 2), 0, 1000, 0, 7);
    } else {
      probSeq = readMux(MUX_SIG2, 2);
    }
  }
  divisionA = mapToDivisions(divisionPotA + cvDivA);
  divisionB = mapToDivisions(divisionPotB + cvDivB);
  probSeq = map(probPot + cvProb, 0, 1023, 0, 100);
}

int mapToDivisions(int value) {
  if (value < 146) return 1;
  if (value < 292) return 2;
  if (value < 438) return 3;
  if (value < 584) return 4;
  if (value < 730) return 8;
  if (value < 876) return 12;
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
  int seq_a_out = sequenceA[step1];
  int seq_b_out = sequenceB[step2];
  dac.setChannelValue(MCP4728_CHANNEL_A, seq_a_out, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  dac.setChannelValue(MCP4728_CHANNEL_B, seq_b_out, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  int mixedValue = quantize((seq_a_out + seq_b_out) / 2, scale);
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
    case 0: r = 255; g = 255; b = 255; break; // Chromatic - White
    case 1: r = 255; g = 0;   b = 0;   break; // Ionian - Red
    case 2: r = 0;   g = 255; b = 0;   break; // Dorian - Green
    case 3: r = 0;   g = 0;   b = 255; break; // Phrygian - Blue
    case 4: r = 255; g = 255; b = 0;   break; // Lydian - Yellow
    case 5: r = 200; g = 30; b = 0;   break; // Mixolydian - Orange
    case 6: r = 255; g = 0;   b = 255; break; // Aeolian - Magenta
    case 7: r = 0;   g = 255; b = 255; break; // Locrian - Cyan
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
  delay(2);
  int secondValue = readMux(MUX_SIG2, muxChannel);

  return abs(secondValue - firstValue) > 2; // Threshold for movement detection
}


const int scales[8][36] = {
    // Chromatic scale
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
     12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
     24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35},
    // Ionian (Major)
    {0, 0, 2, 2, 4, 5, 5, 7, 7, 9, 9, 11,
     12, 12, 14, 14, 16, 17, 17, 19, 19, 21, 21, 23,
     24, 24, 26, 26, 28, 29, 29, 31, 31, 33, 33, 35},
    // Dorian
    {0, 0, 2, 3, 3, 5, 5, 7, 7, 9, 10, 10,
     12, 12, 14, 15, 15, 17, 17, 19, 19, 21, 22, 22,
     24, 24, 26, 27, 27, 29, 29, 31, 31, 33, 34, 34},
    // Phrygian
    {0, 0, 1, 3, 3, 5, 5, 7, 8, 8, 10, 10,
     12, 12, 13, 15, 15, 17, 17, 19, 20, 20, 22, 22,
     24, 24, 25, 27, 27, 29, 29, 31, 32, 32, 34, 34},
    // Lydian
    {0, 0, 2, 2, 4, 6, 6, 7, 7, 9, 9, 11,
     12, 12, 14, 14, 16, 18, 18, 19, 19, 21, 21, 23,
     24, 24, 26, 26, 28, 30, 30, 31, 31, 33, 33, 35},
    // Mixolydian
    {0, 0, 2, 2, 4, 5, 5, 7, 7, 9, 10, 10,
     12, 12, 14, 14, 16, 17, 17, 19, 19, 21, 22, 22,
     24, 24, 26, 26, 28, 29, 29, 31, 31, 33, 34, 34},
    // Aeolian (Natural Minor)
    {0, 0, 2, 3, 3, 5, 5, 7, 8, 8, 10, 10,
     12, 12, 14, 15, 15, 17, 17, 19, 20, 20, 22, 22,
     24, 24, 26, 27, 27, 29, 29, 31, 32, 32, 34, 34},
    // Locrian
    {0, 0, 1, 3, 3, 5, 6, 6, 8, 8, 10, 10,
     12, 12, 13, 15, 15, 17, 18, 18, 20, 20, 22, 22,
     24, 24, 25, 27, 27, 29, 30, 30, 32, 32, 34, 34}
};

// Quantize function
int quantize(int note, int scale) {
    int semitone = map(note, 0, 1023, 0, 35);
    semitone = scales[scale][semitone];
    return 300 + (int)(semitone * 83.333 + 0.5);
}
