# Vinyl-Digitization
Embedded system that saves music from vinyl record onto SD card as a WAV file.

System Components:
Teenst 4.1 Microcontroller
3.3-5V Breadboard Power Supply
480x320 4" TFT Touchscreen
PCM1808 24-Bit Analog-to-Digital Converter
PCM5122 24-Bit Digital-to-Analog Converter
TPS65131 Boost 
EG-530SD-3F DC Motor
Universal Vinyl Tonearm

Communication protocals:
2 I2S
2 SPI
3-5 GPIO

There are two main software files, one was the initial code that contains the full system inetgration and handling in a single page. The other is a more concise version with header files and classes that I'm still tweaking.

The overall system records utilizes a double buffer framework to save audio playback to an SD card on-board of the Teensy at a sample rate of 44.1khz. System records in 16-Bit audio, but with direct changes to certain register and buffer conditions 24-Bit is also pssible. All control are handled via a touchscreen.

The tonearm picks up the analog signlas from the vinyl's groove and sends then to the PCM1808 that converters them into digital signals over I2S input for the Teensy. With the software, there are two pipelines the converter signals route into. First route toward the second I2S output channel of the Teensy to the PCM5122 DAC for Conversion for the speaker output and volume control. The second route being directly into the created buffers using the Teensy's audio library for audio processing and SD recording.



