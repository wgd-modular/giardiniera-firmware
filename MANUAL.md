# Giardiniera Manual

**Giardiniera** is a dual 8-step sequencer featuring four quantized outputs, independent clock divisions (with CV control), and probability functions for dynamic sequencing. Its open-source firmware supports seven musical scales and range control, providing versatile performance options for your Eurorack system.

### Controls

- **Clock:** Controls internal Clock Rate
- **Clock Div A:** Sets Clock Division for Sequencer A
  _Secondary Function:_ Sets the note range for Sequencer A
- **Clock Div B:** Sets Clock Division for Sequencer B
  _Secondary Function:_ Sets the note range for Sequencer B
- **Scale Button:** While the Scale Button is held, the secondary functions of Clock Div A, Clock Div A, and Sequencer Probability are activated
- **Sequencer Probability:** Controls the probability over which sequence is outputted at the Prob Out jack.  
  _Secondary Function:_ Choose between 7 different quantization scales. Each scale is represented by a color from the encoding below, and the knob position from left-to-right corresponds to the table entries top-to-bottom:
- **Steps 1-8:** Set Voltage for each step of the sequence
  _Secondary Function:_ Step 1: Set sequence length
  
  | LED Color | RGB             | Scale                          |
  |-----------|-----------------|--------------------------------|
  | White     | (255, 255, 255) | Chromatic                      |
  | Red       | (255, 0, 0)     | Ionian                         |
  | Green     | (0, 255, 0)     | Dorian                         |
  | Blue      | (0, 0, 255)     | Phrygian                       |
  | Yellow    | (255, 255, 0)   | Lydian                         |
  | Orange    | (200, 30, 0)    | Mixolydian                     |
  | Magenta   | (255, 0, 255)   | Aeolian                        |
  | Cyan      | (0, 255, 255)   | Locrian                        |



### Inputs

- **Clock In:** Clock Input for external Clock
- **Reset A:** Resets Sequencer A to Step 1
- **Reset B:** Resets Sequencer B to Step 1
- **CV:** CV for Sequencer Probability
- **Clock Div A (CV):** CV for Clock Div A
- **Clock Div B (CV):** CV for Clock Div B

### Outputs

- **Clock Out:** Mimics Clock Input
- **A < B:** Compares the current step of Sequence A with Sequence B and, depending on which value is larger, outputs triggers with the Clock Division of A or B
- **Trig A:** Outputs triggers with the Clock Division of Sequence A
- **Trig B:** Outputs triggers with the Clock Division of Sequence B
- **Mix Out:** Outputs a mix of the currently active step of both sequences
- **Prob Out:** Randomly outputs an active step of either sequence. Probability is controlled via the Sequencer Probability pot and CV
- **Seq A Out:** Outputs Sequence A
- **Seq B Out:** Outputs Sequence B
