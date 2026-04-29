//#pragma once
#include <Audio.h>
#include <SD.h>
#include <Arduino.h>

class Record {
public:

  Record(AudioRecordQueue &qL, AudioAmplifier &gL, AudioRecordQueue &qR, AudioAmplifier &gR);

  void begin(); // any init if needed
  bool startRecording(const char *filename = "recording.wav");
  void processRecording(); // call frequently from loop()
  bool endRecording();
  bool exists(const char *filename);
  bool isRecording() const { return recording; }
  void playerSetup(const int speaker, const int motor, const int rpin);
  void setVolume(float vol = 1.0);
  void speakerEN(bool speak);
  void motorEN(bool on);
  void plays(bool play);
  void setRPM(int rpm);
  bool isSD();

private:
  void writeWavHeader(File &file, uint32_t sampleRate, uint32_t dataBytes);
  File audioFile;

  AudioRecordQueue &queueL;
  AudioAmplifier &gainL;

  AudioRecordQueue &queueR;
  AudioAmplifier &gainR;

  volatile int bufferAIndex = 0;
  volatile int bufferBIndex = 0;
  volatile bool bufferAReady = false;
  volatile bool bufferBReady = false;

  volatile bool useA = true;
  volatile bool recording = false;

  static const int SAMPLE_RATE = 44100;
  static const int BYTES_PER_BLOCK = 512;
  static const int BUFFER_SIZE = 4 * BYTES_PER_BLOCK;

  uint8_t bufferA[BUFFER_SIZE];
  uint8_t bufferB[BUFFER_SIZE];

  uint32_t totalBytesWritten = 0;
  
  int SPEAKER_EN;
  int MOTOR_EN;
  int RPM_PIN;
};
