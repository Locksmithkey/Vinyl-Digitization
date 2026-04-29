//#pragma once
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>
#include <vector>
#include <string>
#include <SPI.h>

using namespace std;

class Screen : public TFT_eSPI{
public:
  Screen(uint8_t csPin, uint8_t irqPin, TFT_eSPI &display);

  bool touched();
  bool getMappedPoint(int &x, int &y);
  void messageScreen(String message, int textSize = 20);
  void setRotation(uint8_t rot);
  void begin();

private:
  XPT2046_Touchscreen ts;
  TFT_eSPI &tft;
  uint8_t rotation;
  int mapX(int raw);
  int mapY(int raw);
};

class Button {

public: 
  int x, y, w, h;
  String label;
  uint16_t color;

  Button(int _x, int _y, int _w, int _h, String _label, uint16_t _color)
    : x(_x), y(_y), w(_w), h(_h), label(_label), color(_color) {}

  void draw(TFT_eSPI &tft);
  bool contains(int px, int py);
};

class Slider {

public: 
  int x, y, w, h;
  uint16_t color;

  Slider(int x, int y, int w, int h, uint16_t color) : x(x), y(y), w(w), h(h), color(color){}

  void inc();
  void dec();
  void setRatio(int r);
  void draw(TFT_eSPI &tft);

private:
  int dw;
  int ratio;
};

class MenuScreen {
public:

  MenuScreen(String title) : title(title){}

  void addButton(int x, int y, int w, int h, String label, uint16_t color);
  void addSlider(int sx, int sy, int sw, int sh, uint16_t color);
  void slideUp();
  void slideDown();
  void draw(TFT_eSPI &tft);
  int handleTouch(int tx, int ty);

private:

  vector<Button> buttons;
  vector<Slider> sliders;
  String title;
};