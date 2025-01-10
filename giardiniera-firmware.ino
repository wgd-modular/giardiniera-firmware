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

#define CLOCK_IN 9 // PB1
#define GATE_OUT1 5 // PD5
#define GATE_OUT2 6 // PD6
#define GATE_OUT3 7 // PD7

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_MCP4728 dac;

volatile bool clockState = LOW;
volatile bool lastClockState = LOW;
int step1 = 0;
int step2 = 0;

int sequenceA[NUM_LEDS];
int sequenceB[NUM_LEDS];
int divSeqA = 2;
int divSeqB = 4;
int probSeq = 50;
int cvDivA = 0;
int cvDivB = 0;
int cvProb = 0;

void setup() {
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  startupAnimation(); // Play the startup animation

  // Initialize GPIOs
  pinMode(CLOCK_IN, INPUT);
  pinMode(GATE_OUT1, OUTPUT);
  pinMode(GATE_OUT2, OUTPUT);
  pinMode(GATE_OUT3, OUTPUT);

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

  if (lastClockState == LOW && clockState == HIGH) {
    // Rising edge detected
    handleClockPulse();
  }

  // Update controls
  readControls();

  lastClockState = clockState;
}

void handleClockPulse() {
  // Mirror clock on gate outputs with the same gate width
  digitalWrite(GATE_OUT1, HIGH);
  digitalWrite(GATE_OUT2, HIGH);
  digitalWrite(GATE_OUT3, HIGH);
  delay(10); // Adjustable for gate width
  digitalWrite(GATE_OUT1, LOW);
  digitalWrite(GATE_OUT2, LOW);
  digitalWrite(GATE_OUT3, LOW);

  // Advance sequence steps based on clock division
  static int counterA = 0, counterB = 0;

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
}

void updateLEDs() {
  strip.clear();

  for (int i = 0; i < NUM_LEDS; i++) {
    if (i == step1 && i == step2) {
      // If both sequences are on the same step, mix colors
      strip.setPixelColor(i, strip.Color(128, 0, 128)); // Purple as a mix of Sequence 1 and 2 colors
    } else if (i == step1) {
      // Sequence 1: Cyan
      strip.setPixelColor(i, strip.Color(0, 255, 255));
    } else if (i == step2) {
      // Sequence 2: Lime Green
      strip.setPixelColor(i, strip.Color(0, 255, 0));
    }
  }

  strip.show();
}

void readControls() {
  // Read pots for sequences A and B
  for (int i = 0; i < NUM_LEDS; i++) {
    sequenceA[i] = readMux(MUX_SIG0, i) / 16; // Scale 0-1023 to 0-63
    sequenceB[i] = readMux(MUX_SIG1, i) / 16;
  }

  // Read other controls from the third multiplexer
  divSeqA = map(readMux(MUX_SIG2, 0), 0, 1023, 2, 16); // Map to 2, 4, 8, 16
  divSeqB = map(readMux(MUX_SIG2, 1), 0, 1023, 2, 16);
  probSeq = map(readMux(MUX_SIG2, 2), 0, 1023, 0, 100); // Probability 0-100%
  cvDivA = readMux(MUX_SIG2, 3);
  cvDivB = readMux(MUX_SIG2, 4);
  cvProb = readMux(MUX_SIG2, 5);
}

int readMux(int sigPin, int channel) {
  digitalWrite(MUX_S0, bitRead(channel, 0));
  digitalWrite(MUX_S1, bitRead(channel, 1));
  digitalWrite(MUX_S2, bitRead(channel, 2));
  delayMicroseconds(2); // Allow settling time
  return analogRead(sigPin);
}

void outputSequenceToDAC() {
  dac.setChannelValue(MCP4728_CHANNEL_A, sequenceA[step1] << 4); // Scale 0-255 to 0-4095
  dac.setChannelValue(MCP4728_CHANNEL_B, sequenceB[step2] << 4);
  dac.setChannelValue(MCP4728_CHANNEL_C, random(0, 4096)); // Random value
  dac.setChannelValue(MCP4728_CHANNEL_D, random(0, 4096)); // Random value
}

void startupAnimation() {
  // Pulsing animation with RGB wavy effect, max duration ~3 seconds
  for (int pulse = 0; pulse < 6; pulse++) { // 6 pulses, ~3 seconds
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
