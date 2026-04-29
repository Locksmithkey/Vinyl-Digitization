#include <TFT_eSPI.h>
#include <Adafruit_PCM51xx.h>
#include <XPT2046_Touchscreen.h>

#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <Audio.h>

#include <vector>
#include <string>

using namespace std;

//TFT Config
#define TOUCH_CS 37
#define TOUCH_IRQ 15

#define MOTOR_EN 30
#define SPEAKER_EN 31
#define RPM_PIN6 10
#define RPM_PIN 9

#define PCM5122_ADDR 0x4C

#define TEXTSIZE 2

//Perihpheral Objects
Adafruit_PCM51xx pcm;
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
TFT_eSPI tft = TFT_eSPI();

AudioInputI2S            i2sIn;
AudioOutputI2S2           i2sOut;
AudioRecordQueue         queueL;
AudioRecordQueue         queueR;

AudioAmplifier gainL;
AudioAmplifier gainR;

AudioConnection patchCord1(i2sIn, 1, gainR, 0);   // Right in → gain
AudioConnection patchCord2(gainR, 0, i2sOut, 1);  // gain → Right out
AudioConnection patchCord3(i2sIn, 1, queueR, 0);

AudioConnection patchCord4(i2sIn, 0, gainL, 0);   // Left in → gain
AudioConnection patchCord5(gainL, 0, i2sOut, 0);  // gain → Left out
AudioConnection patchCord6(i2sIn, 0, queueL, 0);

File audioFile;

// Total audio bytes written (for WAV header)
uint32_t totalBytesWritten = 0;

const int SAMPLE_RATE = 44100;
const int BYTES_PER_BLOCK = 512;
const int BUFFER_SIZE = 4 * BYTES_PER_BLOCK;
const int MESSAGEH = tft.width() / 2;
const int MESSAGEW = tft.height() / 2;

uint8_t bufferA[BUFFER_SIZE];
uint8_t bufferB[BUFFER_SIZE];

// Write indices for each buffer
volatile int bufferAIndex = 0;
volatile int bufferBIndex = 0;

// Flags indicating buffer is full and ready to write
volatile bool bufferAReady = false;
volatile bool bufferBReady = false;

// Which buffer we are currently filling
bool useA = true;
bool recording = false;
bool play = false;
bool speak = false;
bool motor = false;

float volume = 1.0;

class Button {
public:
  int x, y, w, h;
  String label;
  uint16_t color;

  Button(int _x, int _y, int _w, int _h, String _label, uint16_t _color)
    : x(_x), y(_y), w(_w), h(_h), label(_label), color(_color) {}

  void draw(TFT_eSPI &tft) {
    tft.fillRoundRect(x, y, w, h, 15, color);
    tft.setTextColor(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString(label, x + w / 2, y + h / 2);
  }

  bool contains(int px, int py) {
    return (px >= x && px <= x + w && py >= y && py <= y + h);
  }
};

class Slider {
public:
  int x, y, w, h;
  uint16_t color;
  int dw = 0;
  int ratio = 10;

  Slider(int x, int y, int w, int h, uint16_t color)
    : x(x), y(y), w(w), h(h), color(color) {}

void inc() {
  dw += (w / ratio);
  if (dw > w) dw = w;
}

void dec() {
  dw -= (w / ratio);
  if (dw < 0) dw = 0;
}

void draw(TFT_eSPI &tft) {
  tft.drawRect(x - 1, y - 1, w + 2, h + 2, TFT_GREEN);
  tft.fillRect(x, y, w, h, TFT_WHITE);     // background
  tft.fillRect(x, y, dw, h, color);        // fill
}
};

class MenuScreen {
public:

  vector<Button> buttons;
  vector<Slider> sliders;
  String title;

  MenuScreen(String _title)
    : title(_title) {}

  void addButton(int x,int y, int w, int h, String label, uint16_t color) {
    Button b = Button(x, y, w, h, label, color);
    buttons.push_back(b);
  }

  void addSlider(int sx, int sy, int sw, int sh, uint16_t color) {
    Slider s = Slider(sx, sy, sw, sh, color);
    sliders.push_back(s);
  }

  void slideUp() {
    sliders[0].inc();
  }

  void slideDown() {
    sliders[0].dec();
  }

  void draw(TFT_eSPI &tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(title, tft.width() / 2, 10);

    for(auto &s : sliders)
      s.draw(tft);

    for(auto &b : buttons)
      b.draw(tft);
  }

  int handleTouch(int tx, int ty) {
    for (int i = 0; i < (int)buttons.size(); i++) {
      if (buttons[i].contains(tx, ty))
        return i;  // return index of button pressed
    }
    return -1;
  }
};


MenuScreen homeMenu("Home");
MenuScreen rpmMenu("RPM Selector");
MenuScreen SDFileMenu("SD");
MenuScreen settingsMenu("Settings");
MenuScreen recordingMenu("Recording...");

MenuScreen *currentMenu = &homeMenu;

void setupHomeMenu() {
  homeMenu.addButton(365, 115, 114, 200, "RPM", TFT_BLUE);
  homeMenu.addButton(245, 115, 114, 200, "Settings", TFT_GREEN);
  homeMenu.addButton(125, 115, 114, 200, "SD Files", TFT_CYAN);
  homeMenu.addButton(5, 115, 114, 200, "Play", TFT_RED);

  homeMenu.addButton(369, 50, 50, 30, "+", TFT_WHITE);
  homeMenu.addButton(61, 50, 50, 30, "-", TFT_WHITE);
  homeMenu.addSlider(115, 50, 250, 30, TFT_RED);
}

void setupRPMMenu() {
  rpmMenu.addButton(5, 115, 114, 200, "33", TFT_BLUE);
  rpmMenu.addButton(125, 115, 114, 200, "45", TFT_GREEN);
  rpmMenu.addButton(245, 115, 114, 200, "78", TFT_CYAN);
  rpmMenu.addButton(365, 115, 114, 200, "Back", TFT_RED);
}

void setupSDFileMenu() {
  SDFileMenu.addButton(245, 115, 114, 200, "New WAV", TFT_BLUE);
  SDFileMenu.addButton(365, 115, 114, 200, "Back", TFT_RED);
}

void setupSettingsMenu() {
  settingsMenu.addButton(125, 115, 114, 200, "Speakers", TFT_BLUE);
  settingsMenu.addButton(245, 115, 114, 200, "Motor", TFT_GREEN);
  settingsMenu.addButton(365, 115, 114, 200, "Back", TFT_RED);
}

void setupRecordingMenu() {
  recordingMenu.addButton(365, 115, 114, 200, "Back", TFT_RED);
}

void dacSetup (){
  pcm.enablePLL(true);
  pcm.setI2SFormat(PCM51XX_I2S_FORMAT_I2S);
  pcm.setI2SSize(PCM51XX_I2S_SIZE_16BIT);
  pcm.setPLLReference(PCM51XX_PLL_REF_BCK);
  pcm.setDACSource(PCM51XX_DAC_CLK_BCK);
  pcm.begin();
}

void setup() {
  //Hardware Setup
  Serial.begin(115200);

  
  //Graphics Setup
  tft.init();
  tft.setRotation(1);
  
  //Touchscreen setup
  ts.begin();
  ts.setRotation(3);

  //DAC Setup
  dacSetup();
  gainL.gain(volume);
  gainR.gain(volume);

  //UI Menu Setup
  setupHomeMenu();
  setupRPMMenu();
  setupSettingsMenu();
  setupSDFileMenu();
  setupRecordingMenu();

  pinMode(MOTOR_EN, OUTPUT);
  pinMode(SPEAKER_EN, OUTPUT);
 // pinMode(RPM_PIN45, OUTPUT);
 // pinMode(RPM_PIN78, OUTPUT);
  pinMode(RPM_PIN, INPUT_PULLUP);

  digitalWrite(MOTOR_EN, LOW);

  AudioMemory(60);

  currentMenu->draw(tft);
}

void loop() {

  if(recording) processRecording();

  if(!ts.touched()) return;

  TS_Point p = ts.getPoint();

  int x = map(p.x, 200, 3800, 0, tft.width());
  int y = map(p.y, 200, 3800, 0, tft.height());

  int pressed = currentMenu->handleTouch(x, y);

  if(pressed < 0) return;

  if (currentMenu == &homeMenu) {
    if (pressed == 0) currentMenu = &rpmMenu;
    if (pressed == 1) currentMenu = &settingsMenu;
    if (pressed == 2) currentMenu = &SDFileMenu;
    if (pressed == 3) {
      play = !play;
      plays(play);
    }
    if (pressed == 4) { volume += 2.0; setVolume(volume); homeMenu.slideUp(); }//volUp();
    if (pressed == 5) { volume -= 2.0; setVolume(volume); homeMenu.slideDown(); } //volDown();
  }

  else if (currentMenu == &rpmMenu) {
    if (pressed == 0) { setRPM(33); currentMenu = &homeMenu;}
    if (pressed == 1) { setRPM(45); currentMenu = &homeMenu; }
    if (pressed == 2) { setRPM(78); currentMenu = &homeMenu; }
    if (pressed == 3) currentMenu = &homeMenu;
  }

  else if (currentMenu == &settingsMenu) {
    if (pressed == 0) {
      speak = !speak;
      speakerEN(speak);
    }
    if(pressed == 1) {
       motor = !motor;
      motorEN(motor);
    }
    if (pressed == 2) currentMenu = &homeMenu;  
  }

  else if (currentMenu == &SDFileMenu) {
    if (pressed == 0) {
      plays(true);
      startRecording();
    }
    if (pressed == 1) currentMenu = &homeMenu;
  }

  else if(currentMenu == &recordingMenu) {
    if (pressed == 0) {
      plays(false);
      endRecording();
      currentMenu = &SDFileMenu;   
    }
  }

  currentMenu->draw(tft);
}

void setVolume(float vol) {
  gainR.gain(vol);
  gainL.gain(vol);
}

void setRPM(int rpm) {
  if(rpm == 33) {
    pinMode(RPM_PIN, INPUT_PULLUP);
   }

  if(rpm == 45) {
    pinMode(RPM_PIN, OUTPUT);
    digitalWrite(RPM_PIN, HIGH);
  }

  if(rpm == 78) {
    pinMode(RPM_PIN, OUTPUT);
    digitalWrite(RPM_PIN, LOW);
  }
}

void messageScreen(String message, int textSize, int x, int y) {

  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(textSize);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(message, x, y);
  delay(2000);
  tft.setTextSize(1);
}

void speakerEN(bool s) {
  speak = s;
  if(s)
    digitalWrite(SPEAKER_EN, HIGH);
  else
    digitalWrite(SPEAKER_EN, LOW);
}

void motorEN(bool m) {
  motor = m;
  if(m)
    digitalWrite(MOTOR_EN, HIGH);
  else
    digitalWrite(MOTOR_EN, LOW);
}

void plays(bool p) {
  play = p;
  motorEN(p);
  speakerEN(p);
}

void writeWavHeader(File &file, uint32_t sampleRate, uint32_t dataBytes) {
    uint32_t chunkSize = 36 + dataBytes;
    uint16_t audioFormat = 1;   // PCM
    uint16_t numChannels = 2;
    uint16_t bitsPerSample = 16;
    uint32_t byteRate = sampleRate * numChannels * 2;
    uint16_t blockAlign = numChannels * 2;

    file.seek(0);

    // RIFF chunk
    file.write("RIFF", 4);
    file.write((uint8_t*)&chunkSize, 4);
    file.write("WAVE", 4);

    // fmt chunk
    file.write("fmt ", 4);
    uint32_t subChunk1Size = 16;
    file.write((uint8_t*)&subChunk1Size, 4);
    file.write((uint8_t*)&audioFormat, 2);
    file.write((uint8_t*)&numChannels, 2);
    file.write((uint8_t*)&sampleRate, 4);
    file.write((uint8_t*)&byteRate, 4);
    file.write((uint8_t*)&blockAlign, 2);
    file.write((uint8_t*)&bitsPerSample, 2);

    // data chunk
    file.write("data", 4);
    file.write((uint8_t*)&dataBytes, 4);
}

void endRecording() {

  if(!recording) return;

  recording = false;

    // Stop queues so no more audio arrives
    queueL.end();
    queueR.end();

    // Flush any partially filled buffers
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

    // First write any full buffers that were marked ready
    if (aReady) {
        audioFile.write(bufferA, BUFFER_SIZE);
        totalBytesWritten += BUFFER_SIZE;
    }
    if (bReady) {
        audioFile.write(bufferB, BUFFER_SIZE);
        totalBytesWritten += BUFFER_SIZE;
    }

    // Then write any remaining partial data
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
    
    
    if(SD.exists("recording.wav"))
      messageScreen("File Successfully Recorded", TEXTSIZE, MESSAGEW, MESSAGEH);
}

void startRecording() {

  recording = true;

  if(!SD.begin(BUILTIN_SDCARD)) {

    messageScreen("SD Card Not Inserted", TEXTSIZE, MESSAGEW, MESSAGEH);

    currentMenu = &SDFileMenu;
    return;
  }

  currentMenu = &recordingMenu;
  //Eventually add file naming
  audioFile = SD.open("recording.wav", FILE_WRITE);
  audioFile.seek(44);

  queueL.begin();
  queueR.begin();
}

void processRecording() {

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

        // Drop if buffer not yet written
        if (*readyFlag) {
            queueL.freeBuffer();
            queueR.freeBuffer();
            continue;
        }

        int idx = *indexPtr;

        // Ensure space for one full block (512 bytes)
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

        // 16-bit interleaved write
        for (int i = 0; i < 128; i++) {
            int16_t left  = l[i];
            int16_t right = r[i];

            // Left (LSB first)
            target[idx++] = left & 0xFF;
            target[idx++] = (left >> 8) & 0xFF;

            // Right (LSB first)
            target[idx++] = right & 0xFF;
            target[idx++] = (right >> 8) & 0xFF;
        }

        *indexPtr = idx;

        // If buffer full, mark ready
        if (idx >= BUFFER_SIZE) {
            noInterrupts();
            *readyFlag = true;
            interrupts();
            useA = !useA;
        }

        queueL.freeBuffer();
        queueR.freeBuffer();
    }

    // -------- SD WRITE --------

    if (bufferAReady) {
        noInterrupts();
        bufferAReady = false;
        int bytesToWrite = BUFFER_SIZE;
        bufferAIndex = 0;
        interrupts();

        audioFile.write(bufferA, bytesToWrite);
        totalBytesWritten += bytesToWrite;
    }

    if (bufferBReady) {
        noInterrupts();
        bufferBReady = false;
        int bytesToWrite = BUFFER_SIZE;
        bufferBIndex = 0;
        interrupts();

        audioFile.write(bufferB, bytesToWrite);
        totalBytesWritten += bytesToWrite;
    }

    if (totalBytesWritten > (SD.totalSize() - SD.usedSize())) {
        messageScreen("File Not Saved. Not Enough Storage.", TEXTSIZE, MESSAGEW, MESSAGEH);
        writeWavHeader(audioFile, SAMPLE_RATE, totalBytesWritten);
        recording = false;
        return;
    }
}


