#include "Screen.h"

Screen::Screen(uint8_t csPin, uint8_t irqPin, TFT_eSPI &display)
  : ts(csPin, irqPin), tft(display) {}

void Screen::begin() {
  tft.init();
  ts.begin();
  tft.setRotation(1);
  ts.setRotation(3);
}
void Screen :: messageScreen(String message, int textSize) {

  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(textSize);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(message, tft.width() / 2, tft.height() / 2);
  delay(3000);
  tft.setTextSize(1);
  
}
void Screen::setRotation(uint8_t rot) {
  rotation = rot;
  tft.setRotation(1); // keep your mapping if needed
  ts.setRotation(rotation);
}

bool Screen::touched() {
  return ts.touched();
}

bool Screen::getMappedPoint(int &x, int &y) {
  if (!ts.touched()) return false;
  TS_Point p = ts.getPoint();
  // adjust these raw ranges if your panel differs
  x = map(p.x, 200, 3800, 0, tft.width());
  y = map(p.y, 200, 3800, 0, tft.height());
  x = constrain(x, 0, tft.width());
  y = constrain(y, 0, tft.height());
  return true;
}

void MenuScreen :: addButton(int x, int y, int w, int h, String label, uint16_t color) {
    Button b = Button(x, y, w, h, label, color);
    buttons.push_back(b);
  }

void MenuScreen :: addSlider(int sx, int sy, int sw, int sh, uint16_t color) {
 Slider s = Slider(sx, sy, sw, sh, color);
 sliders.push_back(s);
}

void MenuScreen :: slideUp() {
  for (auto &s : sliders) {
    s.inc();
  }
}

void MenuScreen :: slideDown() {
  for (auto &s : sliders) {
    s.dec();
  }
}

void MenuScreen :: draw(TFT_eSPI &tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(title, tft.width() / 2, 10);

  for(auto &b : buttons)
    b.draw(tft);
}

int MenuScreen :: handleTouch(int tx, int ty) {
    for (int i = 0; i < (int)buttons.size(); i++) {
      if (buttons[i].contains(tx, ty))
        return i;  // return index of button pressed
    }
    return -1;
  }

void Button :: draw(TFT_eSPI &tft) {
    tft.fillRoundRect(x, y, w, h, 15, color);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(label, x + w / 2, y + h / 2);
  }

bool Button :: contains(int px, int py) {
  return (px >= x && px <= x + w && py >= y && py <= y + h);
}

void Slider :: inc() {
   dw += (w / ratio);
   if (dw > w) dw = w;
}

void Slider :: dec() {
   dw -= (w / ratio);
   if (dw < 0) dw = 0;
}

void Slider :: setRatio(int r) {
  ratio = r;
  dw = 0;
}

void Slider :: draw(TFT_eSPI &tft) {
  tft.fillRect(x, y, w, h, TFT_WHITE);
  tft.drawRect(x - 1, y - 1, w + 2, h + 2, TFT_GREEN);
  tft.fillRect(x, y, dw, h, color);
}