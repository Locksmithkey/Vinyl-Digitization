#include "Screen.h"
#include "Record.h"
#include <TFT_eSPI.h>
#include <Adafruit_PCM51xx.h>

#define TOUCH_CS 37
#define TOUCH_IRQ 15

int speakerEN = 1;
int motorEN = 5;

float vol = 1.0;

bool on = false;
bool motor = false;
bool play = false;

// existing AudioRecordQueue queueL, queueR; (keep these as before)
AudioRecordQueue queueR;
AudioRecordQueue queueL;
AudioAmplifier gainR;
AudioAmplifier gainL;

TFT_eSPI tft = TFT_eSPI();

Screen screen(TOUCH_CS, TOUCH_IRQ, tft);
Record recorder(queueL, gainL, queueR, gainR);

Adafruit_PCM51xx pcm;

MenuScreen homeMenu("Home");
MenuScreen rpmMenu("RPM");
MenuScreen SDFileMenu("SD");
MenuScreen settingsMenu("Settings");
MenuScreen recordingMenu("Recording...");

MenuScreen *currentMenu = &homeMenu;

void setupHomeMenu() {
  homeMenu.addButton(365, 115, 114, 200, "RPM", TFT_BLUE);
  homeMenu.addButton(245, 115, 114, 200, "Settings", TFT_GREEN);
  homeMenu.addButton(125, 115, 114, 200, "SD Files", TFT_CYAN);
  homeMenu.addButton(5, 115, 114, 200, "Play", TFT_RED);

  homeMenu.addButton(384, 50, 50, 30, "+", TFT_WHITE);
  homeMenu.addButton(76, 50, 50, 30, "-", TFT_WHITE);
  homeMenu.addSlider(130, 50, 250, 30, TFT_RED);
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
  // put your setup code here, to run once:
  Serial.begin(115200);

  tft.begin();

  screen.begin();
  screen.setRotation(3);

  setupHomeMenu();
  setupRPMMenu();
  setupSettingsMenu();
  setupSDFileMenu();
  setupRecordingMenu();

  //recorder.begin();
  recorder.playerSetup(speakerEN, motorEN, 9);
  recorder.speakerEN(false);
  recorder.speakerEN(false);

  currentMenu->draw(tft);
}

void loop() {

int x, y;

 if (recorder.isRecording()) recorder.processRecording();
 if (!screen.getMappedPoint(x, y)) return;

  screen.getMappedPoint(x,y);

  int pressed = currentMenu->handleTouch(x, y);

  if(pressed < 0) return;

      // HOME
    if (currentMenu == &homeMenu) {
        //tft.drawRect(199, 30, 82,20, TFT_WHITE);
      if (pressed == 0) currentMenu = &rpmMenu;
      if (pressed == 1) currentMenu = &settingsMenu;
      if (pressed == 2) currentMenu = &SDFileMenu;
      if (pressed == 3) {
        play = !play;
        recorder.plays(play);
      }
      if (pressed == 4) { homeMenu.slideUp(); vol += 2.0; recorder.setVolume(vol); }//volUp();
      if (pressed == 5) { homeMenu.slideDown(); vol -= 2.0; recorder.setVolume(vol); }
    }

    else if (currentMenu == &rpmMenu) {
      if (pressed == 0) currentMenu = &homeMenu;//setRPM(RPM[0]); 33
      if (pressed == 1) currentMenu = &homeMenu;//setRPM(RPM[1]); 45
      if (pressed == 2) currentMenu = &homeMenu;//setRPM(RPM[2]); 78
      if (pressed == 3) currentMenu = &homeMenu;
    }

    else if (currentMenu == &settingsMenu) {
      if (pressed == 0) {
        on = !on;
        recorder.speakerEN(on);
      }
      if(pressed == 1) {
        motor = !motor;
        recorder.motorEN(motor);
      }
      if (pressed == 2) currentMenu = &homeMenu;
      //sleep time
      //format sd card
    }

    else if (currentMenu == &SDFileMenu) {
      if (pressed == 0) {
        if(!SD.begin(BUILTIN_SDCARD))
        recorder.plays(true);
      
        if(recorder.startRecording())
          currentMenu = &recordingMenu;
        else
          screen.messageScreen("SD Card Not Inserted"); 
      }
      if (pressed == 1) currentMenu = &homeMenu;
    }

    else if(currentMenu == &recordingMenu) {
      if (pressed == 0) {
        recorder.plays(false);
        recorder.endRecording() ? screen.messageScreen("File Saved Succesfully") : screen.messageScreen("File Not Saved");
        currentMenu = &SDFileMenu;   
      }
    }
  // when starting recording:
  // if (!recorder.startRecording("recording.wav")) { messageScreen("SD Card Not Inserted", TEXTSIZE, tft.width()/2, tft.height()/2); currentMenu = &SDFileMenu; }
    currentMenu->draw(tft);
}
