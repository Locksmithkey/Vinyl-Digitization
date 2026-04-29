#include "Record.h"

/*Record::Record(AudioRecordQueue &qL, AudioRecordQueue &qR)
  : queueL(qL), queueR(qR) { 
    AudioInputI2S IN;
    AudioConnection patch1(IN, 0, qL, 0);
    AudioConnection patch2(IN, 1, qR, 0);} */

Record :: Record(AudioRecordQueue &qL, AudioAmplifier &gL, AudioRecordQueue &qR, AudioAmplifier &gR)
  : queueL(qL), gainL(gL), queueR(qR), gainR(gR) {
    AudioInputI2S IN;
    AudioOutputI2S2 OUT;

    AudioConnection patchCord1(IN, 1, gR, 0);   // Right in → gain
    AudioConnection patchCord2(gR, 0, OUT, 1);  // gain → Right out
    AudioConnection patchCord3(IN, 1, qR, 0);

    AudioConnection patchCord4(IN, 0, gL, 0);   // Left in → gain
    AudioConnection patchCord5(gL, 0, OUT, 0);  // gain → Left out
    AudioConnection patchCord6(IN, 0, qL, 0); }

void Record::begin() {
  // nothing required now; placeholder for future init
}

void Record :: setVolume(float vol) {
  gainR.gain(vol);
  gainL.gain(vol);
}

bool Record :: exists(const char *filename) {
  return SD.exists(filename);
}

bool Record::startRecording(const char *filename) {
  if (!SD.begin(BUILTIN_SDCARD)) {
    return false;
  }
  audioFile = SD.open(filename, FILE_WRITE);
  if (!audioFile) return false;
  audioFile.seek(44); // reserve header
  totalBytesWritten = 0;
  bufferAIndex = bufferBIndex = 0;
  bufferAReady = bufferBReady = false;
  useA = true;
  recording = true;
  queueL.begin();
  queueR.begin();
  return true;
}
void Record :: playerSetup(int speaker, int motor, int rpin) {
  RPM_PIN = rpin;
  SPEAKER_EN = speaker;
  MOTOR_EN = motor;
  pinMode(SPEAKER_EN, OUTPUT);
  pinMode(MOTOR_EN, OUTPUT);
  digitalWrite(SPEAKER_EN, LOW);
  digitalWrite(MOTOR_EN, LOW);
}

void Record :: speakerEN(bool speak) {
  
  if(speak)
    digitalWrite(SPEAKER_EN, HIGH);
  else
    digitalWrite(SPEAKER_EN, LOW);
}

void Record :: motorEN(bool on) {

  if(on)
    digitalWrite(MOTOR_EN, HIGH);
  else
    digitalWrite(MOTOR_EN, LOW);
}

void Record :: plays(bool play) {

  if(!play) {
    digitalWrite(SPEAKER_EN, LOW);
    digitalWrite(MOTOR_EN, LOW);
  }
  else {
    digitalWrite(SPEAKER_EN, HIGH);
    digitalWrite(MOTOR_EN, HIGH);
  }
}

bool Record::endRecording() {
  if (!recording) return false;
  recording = false;

  queueL.end();
  queueR.end();

  noInterrupts();
  int aIndex = bufferAIndex;
  int bIndex = bufferBIndex;
  bool aReady = bufferAReady;
  bool bReady = bufferBReady;
  bufferAReady = false;
  bufferBReady = false;
  bufferAIndex = 0;
  bufferBIndex = 0;
  interrupts();

  if (aReady) {
    audioFile.write(bufferA, BUFFER_SIZE);
    totalBytesWritten += BUFFER_SIZE;
  }
  if (bReady) {
    audioFile.write(bufferB, BUFFER_SIZE);
    totalBytesWritten += BUFFER_SIZE;
  }
  if (aIndex > 0 && !aReady) {
    audioFile.write(bufferA, aIndex);
    totalBytesWritten += aIndex;
  }
  if (bIndex > 0 && !bReady) {
    audioFile.write(bufferB, bIndex);
    totalBytesWritten += bIndex;
  }

  audioFile.flush();
  writeWavHeader(audioFile, SAMPLE_RATE, totalBytesWritten);
  audioFile.close();

  if(!audioFile) return false;

  return true;
}

void Record::processRecording() {
  if (!recording) return;

  while (queueL.available() && queueR.available()) {
    int16_t *l = queueL.readBuffer();
    int16_t *r = queueR.readBuffer();

    uint8_t *target;
    volatile int *indexPtr;
    volatile bool *readyFlag;

    if (useA) {
      target = bufferA;
      indexPtr = &bufferAIndex;
      readyFlag = &bufferAReady;
    } else {
      target = bufferB;
      indexPtr = &bufferBIndex;
      readyFlag = &bufferBReady;
    }

    if (*readyFlag) {
      queueL.freeBuffer();
      queueR.freeBuffer();
      continue;
    }

    int idx = *indexPtr;

    if (idx + BYTES_PER_BLOCK > BUFFER_SIZE) {
      noInterrupts();
      *readyFlag = true;
      interrupts();
      useA = !useA;

      if (useA) {
        target = bufferA;
        indexPtr = &bufferAIndex;
        readyFlag = &bufferAReady;
      } else {
        target = bufferB;
        indexPtr = &bufferBIndex;
        readyFlag = &bufferBReady;
      }

      if (*readyFlag) {
        queueL.freeBuffer();
        queueR.freeBuffer();
        continue;
      }
      idx = *indexPtr;
    }

    for (int i = 0; i < 128; i++) {
      int32_t left24 = ((int32_t)l[i]) << 8;
      int32_t right24 = ((int32_t)r[i]) << 8;

      target[idx + 0] = left24 & 0xFF;
      target[idx + 1] = (left24 >> 8) & 0xFF;
      target[idx + 2] = (left24 >> 16) & 0xFF;

      target[idx + 3] = right24 & 0xFF;
      target[idx + 4] = (right24 >> 8) & 0xFF;
      target[idx + 5] = (right24 >> 16) & 0xFF;

      idx += 6;
    }

    *indexPtr = idx;

    if (idx >= BUFFER_SIZE) {
      noInterrupts();
      *readyFlag = true;
      interrupts();
      useA = !useA;
    }

    queueL.freeBuffer();
    queueR.freeBuffer();
  }

  if (bufferAReady) {
    noInterrupts();
    bufferAReady = false;
    int bytesToWrite = BUFFER_SIZE;
    bufferAIndex = 0;
    interrupts();

    audioFile.write(bufferA, bytesToWrite);
    totalBytesWritten += bytesToWrite;
    audioFile.flush();
  }

  if (bufferBReady) {
    noInterrupts();
    bufferBReady = false;
    int bytesToWrite = BUFFER_SIZE;
    bufferBIndex = 0;
    interrupts();

    audioFile.write(bufferB, bytesToWrite);
    totalBytesWritten += bytesToWrite;
    audioFile.flush();
  }

  if (totalBytesWritten > (SD.totalSize() - SD.usedSize())) {
    // caller should handle user message; here we finalize file
    writeWavHeader(audioFile, SAMPLE_RATE, totalBytesWritten);
    recording = false;
  }
}

void Record::writeWavHeader(File &file, uint32_t sampleRate, uint32_t dataBytes) {
  uint32_t chunkSize = 36 + dataBytes;
  uint16_t audioFormat = 1;
  uint16_t numChannels = 2;
  uint16_t bitsPerSample = 24;
  uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
  uint16_t blockAlign = numChannels * (bitsPerSample / 8);

  file.seek(0);
  file.write("RIFF", 4);
  file.write((uint8_t*)&chunkSize, 4);
  file.write("WAVE", 4);

  file.write("fmt ", 4);
  uint32_t subChunk1Size = 16;
  file.write((uint8_t*)&subChunk1Size, 4);
  file.write((uint8_t*)&audioFormat, 2);
  file.write((uint8_t*)&numChannels, 2);
  file.write((uint8_t*)&sampleRate, 4);
  file.write((uint8_t*)&byteRate, 4);
  file.write((uint8_t*)&blockAlign, 2);
  file.write((uint8_t*)&bitsPerSample, 2);

  file.write("data", 4);
  file.write((uint8_t*)&dataBytes, 4);
}

bool Record :: isSD () {
  if(!SD.begin(BUILTIN_SDCARD)) return false;

  return true;
}

void Record :: setRPM(int rpm) {
  if(rpm == 33) 
    pinMode(RPM_PIN, INPUT_DISABLE);

  if(rpm == 45) {
    pinMode(RPM_PIN, OUTPUT);
    digitalWrite(RPM_PIN, HIGH);
  }

  if(rpm == 78) {
    pinMode(RPM_PIN, OUTPUT);
    digitalWrite(RPM_PIN, LOW);
  }
}