# UID2MIDI

## Overview

This C++ program serves as a MIDI controller that reads input from a keyboard and triggers MIDI events accordingly. It uses the ALSA library for MIDI communication and captures keyboard events from an input device.

## Prerequisites

- ALSA library installed on your system.
- A compatible MIDI device connected to your computer.
- A supported keyboard for input.

## Installing ALSA Library

Before running the program, ensure that the ALSA library is installed on your system. You can install it using the package manager for your Linux distribution. For example, on Debian-based systems:

``` sudo apt-get install libasound2-dev ``` 

## Compilation

Compile the program using a C++ compiler. For example:

``` g++ midi_controller.cpp -o midi_controller -lasound ``` 

## Usage

1. **Run the Program:**

   Execute the compiled program:

   ``` ./midi_controller ``` 

3. **Connect MIDI Device:**

Ensure that your MIDI device is properly connected to the computer.

3. **Configure MIDI Ports:**

The program will open a MIDI output port named "Out" and connect to the default MIDI destination (port 14). Adjust the MIDI port settings in the code if needed.

4. **Map Keyboard Keys:**

The program maps specific keys on your keyboard to MIDI events. Refer to the "handle_event" function in the code to see the key mappings.

- **Decks:**
  - Decks 1-9: Replace with desired keys.
- **Drum Pads:**
  - Drum Pads 1-9: Replace with desired keys.
- **Note Triggers:**
  - Note Triggers 1-21: Replace with desired keys.

5. **Recording and Looping:**

- **Recording:**
  - Press the "loop" key to start recording MIDI events.
- **Looping:**
  - Press the "loop" key again to stop recording and start looping the recorded events.

6. **Sequencer and Deck Activation:**

- Press the "sequencer" key to start or end the sequencer.
- Press the "deck" keys to activate or mute specific decks.

7. **Exiting the Program:**

- Press the "Esc" key to exit the program. This will stop all MIDI output and close the ALSA connection.

## Notes

- The program assumes a specific keyboard. Modify the `deviceName` variable in the code to match your keyboard's name.
- Adjust ALSA port settings in the code if needed.
- If you are using Windows rethink about your life decisions.

## Credits

This program was developed by Federico Vidoli.
